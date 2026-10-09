"""Safe parser for derived-channel math expressions.

Expressions are parsed with a small recursive-descent parser (instead of eval())
and compiled into polars expressions. Only numeric literals, arithmetic
operators, column references, and a fixed whitelist of math functions are
allowed.

Grammar::

    expression := term (("+" | "-") term)*
    term       := factor (("*" | "/" | "%") factor)*
    factor     := ("-" | "+") factor | power
    power      := primary ("**" factor)?
    primary    := NUMBER
                | FUNCTION "(" expression ("," expression)* ")"
                | IDENTIFIER
                | "(" expression ")"

Operators follow Python precedence (``**`` binds tightest and is right-associative).
Column references may contain dots (ex: ``VCAN.DRIVELINE.front_shockpots.left``).
"""

from __future__ import annotations

import difflib
import math
import re
from dataclasses import dataclass
from enum import Enum, auto

import polars as pl


class ExpressionError(ValueError):
    """Raised when an expression string cannot be tokenized or parsed."""


class _TokenKind(Enum):
    NUMBER = auto()
    IDENT = auto()
    OP = auto()
    LPAREN = auto()
    RPAREN = auto()
    COMMA = auto()


# Human-readable token names for error messages.
_TOKEN_NAMES: dict[_TokenKind, str] = {
    _TokenKind.NUMBER: "a number",
    _TokenKind.IDENT: "a signal name or function",
    _TokenKind.OP: "an operator",
    _TokenKind.LPAREN: "'('",
    _TokenKind.RPAREN: "')'",
    _TokenKind.COMMA: "','",
}


@dataclass(frozen=True)
class _Token:
    kind: _TokenKind
    value: str
    position: int


# name -> number of arguments
_FUNCTIONS: dict[str, int] = {
    name: 1
    for name in (
        "sin",
        "cos",
        "tan",
        "asin",
        "acos",
        "atan",
        "sqrt",
        "exp",
        "log",
        "abs",
        "ceil",
        "floor",
        "radians",
        "degrees",
    )
}
_FUNCTIONS.update({name: 2 for name in ("atan2", "pow", "min", "max")})

_TOKEN_RE = re.compile(
    r"""
    (?P<number>(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)
    |(?P<ident>[A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z0-9_]+)*)
    |(?P<op>\*\*|[+\-*/%])
    |(?P<lparen>\()
    |(?P<rparen>\))
    |(?P<comma>,)
    |(?P<space>\s+)
    """,
    re.VERBOSE,
)


def _tokenize(source: str) -> list[_Token]:
    tokens: list[_Token] = []
    position = 0

    while position < len(source):
        match = _TOKEN_RE.match(source, position)
        if match is None:
            raise ExpressionError(
                f"Unexpected character {source[position]!r} at position {position}"
            )

        kind_name = match.lastgroup
        if kind_name != "space":
            tokens.append(_Token(_TokenKind[kind_name.upper()], match.group(), position))
        position = match.end()

    return tokens


# --- AST node types ----------------------------------------------------------


@dataclass(frozen=True)
class _Number:
    value: float


@dataclass(frozen=True)
class _Ref:
    name: str


@dataclass(frozen=True)
class _Binary:
    op: str
    left: object
    right: object


@dataclass(frozen=True)
class _Unary:
    op: str
    operand: object


@dataclass(frozen=True)
class _Call:
    name: str
    args: tuple


class _Parser:
    def __init__(self, tokens: list[_Token], known_columns: frozenset[str]) -> None:
        self._tokens = tokens
        self._index = 0
        self._known_columns = known_columns

    def parse(self) -> object:
        if not self._tokens:
            raise ExpressionError("Expression is empty")

        node = self._parse_expression()
        if self._index != len(self._tokens):
            token = self._tokens[self._index]
            raise ExpressionError(
                f"Unexpected {token.value!r} at position {token.position}"
            )
        return node

    # --- grammar rules ---

    def _parse_expression(self) -> object:
        node = self._parse_term()
        while (
            self._peek(_TokenKind.OP) is not None
            and self._tokens[self._index].value in ("+", "-")
        ):
            op = self._advance(_TokenKind.OP).value
            node = _Binary(op, node, self._parse_term())
        return node

    def _parse_term(self) -> object:
        node = self._parse_factor()
        while (
            self._peek(_TokenKind.OP) is not None
            and self._tokens[self._index].value in ("*", "/", "%")
        ):
            op = self._advance(_TokenKind.OP).value
            node = _Binary(op, node, self._parse_factor())
        return node

    def _parse_factor(self) -> object:
        token = self._peek(_TokenKind.OP)
        if token is not None and token.value in ("+", "-"):
            op = self._advance(_TokenKind.OP).value
            return _Unary(op, self._parse_factor())
        return self._parse_power()

    def _parse_power(self) -> object:
        node = self._parse_primary()
        if (
            self._peek(_TokenKind.OP) is not None
            and self._tokens[self._index].value == "**"
        ):
            self._advance(_TokenKind.OP)
            node = _Binary("**", node, self._parse_factor())
        return node

    def _parse_primary(self) -> object:
        token = self._peek()
        if token is None:
            raise ExpressionError("Expression ends unexpectedly")

        if token.kind is _TokenKind.NUMBER:
            self._advance(_TokenKind.NUMBER)
            return _Number(float(token.value))

        if token.kind is _TokenKind.LPAREN:
            self._advance(_TokenKind.LPAREN)
            node = self._parse_expression()
            self._advance(_TokenKind.RPAREN)
            return node

        if token.kind is _TokenKind.IDENT:
            self._advance(_TokenKind.IDENT)
            if token.value in _FUNCTIONS:
                return self._parse_call(token.value, token.position)
            if token.value in self._known_columns:
                return _Ref(token.value)
            raise ExpressionError(
                f"Unknown signal {token.value!r} at position {token.position}. "
                + self._unknown_signal_hint(token.value)
            )

        raise ExpressionError(
            f"Unexpected {token.value!r} at position {token.position}"
        )

    def _parse_call(self, name: str, position: int) -> _Call:
        self._advance(_TokenKind.LPAREN)
        args = [self._parse_expression()]
        while self._peek(_TokenKind.COMMA) is not None:
            self._advance(_TokenKind.COMMA)
            args.append(self._parse_expression())

        close = self._advance(_TokenKind.RPAREN)
        expected = _FUNCTIONS[name]
        if len(args) != expected:
            raise ExpressionError(
                f"{name}() at position {position} expects {expected} "
                f"argument(s), got {len(args)}"
            )

        return _Call(name, tuple(args))

    # --- token helpers ---

    def _peek(self, kind: _TokenKind | None = None) -> _Token | None:
        if self._index >= len(self._tokens):
            return None
        token = self._tokens[self._index]
        return token if kind is None or token.kind is kind else None

    def _advance(self, kind: _TokenKind) -> _Token:
        token = self._peek(kind)
        if token is None:
            found = (
                self._tokens[self._index]
                if self._index < len(self._tokens)
                else None
            )
            found_desc = (
                f"{found.value!r} at position {found.position}" if found else "end of expression"
            )
            raise ExpressionError(
                f"Expected {_TOKEN_NAMES[kind]} but found {found_desc}"
            )
        self._index += 1
        return token

    def _unknown_signal_hint(self, name: str) -> str:
        candidates = sorted(self._known_columns | _FUNCTIONS.keys())
        suggestions = difflib.get_close_matches(name, candidates, n=3, cutoff=0.7)
        if suggestions:
            return f"Did you mean: {', '.join(suggestions)}?"
        return (
            "A column reference must name a raw telemetry column or a channel "
            "defined earlier in the signal library."
        )


# --- compilation to polars expressions ---------------------------------------


def _apply_function(name: str, args: list[pl.Expr]) -> pl.Expr:
    if name in ("sin", "cos", "tan", "asin", "acos", "atan", "sqrt", "exp", "log", "abs", "ceil", "floor"):
        return getattr(args[0], name)()
    if name == "radians":
        return pl.lit(math.pi) * args[0] / 180.0
    if name == "degrees":
        return args[0] * 180.0 / pl.lit(math.pi)
    if name == "atan2":
        return pl.arctan2(args[0], args[1])
    if name == "pow":
        return args[0].pow(args[1])
    if name == "min":
        return args[0].min(args[1])
    if name == "max":
        return args[0].max(args[1])
    raise ExpressionError(f"Unsupported function {name!r}")  # pragma: no cover


def _compile(node: object, refs: list[str]) -> pl.Expr:
    if isinstance(node, _Number):
        return pl.lit(node.value)
    if isinstance(node, _Ref):
        refs.append(node.name)
        return pl.col(node.name)
    if isinstance(node, _Unary):
        operand = _compile(node.operand, refs)
        return -operand if node.op == "-" else +operand

    if isinstance(node, _Binary):
        left = _compile(node.left, refs)
        right = _compile(node.right, refs)
        return {
            "+": left + right,
            "-": left - right,
            "*": left * right,
            "/": left / right,
            "%": left % right,
            "**": left**right,
        }[node.op]

    if isinstance(node, _Call):
        args = [_compile(arg, refs) for arg in node.args]
        return _apply_function(node.name, args)
    raise ExpressionError(f"Unsupported expression node {type(node).__name__}")


@dataclass(frozen=True)
class ParsedExpression:
    """A compiled expression plus the column names it references."""

    expression: pl.Expr
    references: tuple[str, ...]


def parse_expression(source: str, known_columns: frozenset[str]) -> ParsedExpression:
    """Parse ``source`` into a polars expression.

    ``known_columns`` is the set of valid column references (raw telemetry
    columns plus channels defined earlier in the library). Anything that is
    neither a known column, a whitelisted function, a number, or an operator
    raises :class:`ExpressionError`.
    """
    if not isinstance(source, str) or not source.strip():
        raise ExpressionError("Expression must be a non-empty string")

    tokens = _tokenize(source)
    tree = _Parser(tokens, known_columns).parse()

    refs: list[str] = []
    expression = _compile(tree, refs)
    return ParsedExpression(expression=expression, references=tuple(dict.fromkeys(refs)))
