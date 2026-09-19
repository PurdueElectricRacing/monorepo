use chrono::Datelike as _;
use chrono::TimeZone as _;

use crate::daq_log_parse::parse::ParsedMessage;

// In that case we use this fallback slope value. It is generally safe to assume a
// slope of 1.0 since the DAQ runs at 1 tick = 1 ms, but in post-processing there
// is no reason not to run a proper linear regression when multiple GPS points exist.
const REGRESSION_FALLBACK_SLOPE: f64 = 1.0;

pub struct CorrelationFunction {
    /// real_time ~= slope * log_time_ms + intercept_ms
    ///
    /// Stored as:
    /// unix_ms = slope * log_ts_ms + intercept_ms
    slope: f64,
    intercept_ms: f64,
}

impl CorrelationFunction {
    pub fn correlate(&self, log_ts: u32) -> Option<chrono::DateTime<chrono::Utc>> {
        let unix_ms = self.slope * log_ts as f64 + self.intercept_ms;
        match chrono::DateTime::from_timestamp_millis(unix_ms.round() as i64) {
            Some(dt) => Some(dt),
            None => {
                log::error!(
                    "Correlated time {} ms for log time {} ms is out of range for chrono::DateTime",
                    unix_ms,
                    log_ts
                );
                None
            }
        }
    }
}

pub struct CorrelationChunkResult {
    pub parsed_msgs: Vec<ParsedMessage>,
    pub correlation_fn: Option<CorrelationFunction>,
}

pub fn time_correlate_chunks(chunks: Vec<Vec<ParsedMessage>>) -> Vec<CorrelationChunkResult> {
    chunks.into_iter().map(time_correlate_chunk).collect()
}

impl CorrelationChunkResult {
    pub fn uncorrelated_new(chunk: Vec<ParsedMessage>) -> Self {
        Self {
            parsed_msgs: chunk,
            correlation_fn: None,
        }
    }

    pub fn correlated_new(chunk: Vec<ParsedMessage>, correlation_fn: CorrelationFunction) -> Self {
        Self {
            parsed_msgs: chunk,
            correlation_fn: Some(correlation_fn),
        }
    }
}

pub fn physical_to_u64(dsv: &can_decode::DecodedSignalValue) -> u64 {
    dsv.physical.round() as u64
}

pub fn time_correlate_chunk(chunk: Vec<ParsedMessage>) -> CorrelationChunkResult {
    // Idea: in the chunk, look for GPS messages which have both a timestamp and a corresponding real time
    // Use those to create a mapping from the log's timestamps to real time, and use that mapping to convert
    // all messages in the chunk to have real timestamps.
    // If the correlation is successful, we return the orginal messages along with a correlation function.
    // If we can't find any GPS messages, or if the correlation fails, return just the original messages.

    // First, find all GPS messages and extract their timestamps and real times
    let mut gps_points = Vec::new();
    for msg in &chunk {
        if msg.decoded.name == "gps_time" {
            let millisecond = msg
                .decoded
                .signals
                .get("millisecond")
                .map(|sig| physical_to_u64(&sig.value));
            let second = msg
                .decoded
                .signals
                .get("second")
                .map(|sig| physical_to_u64(&sig.value));
            let minute = msg
                .decoded
                .signals
                .get("minute")
                .map(|sig| physical_to_u64(&sig.value));
            let hour = msg
                .decoded
                .signals
                .get("hour")
                .map(|sig| physical_to_u64(&sig.value));
            let day = msg
                .decoded
                .signals
                .get("day")
                .map(|sig| physical_to_u64(&sig.value));
            let month = msg
                .decoded
                .signals
                .get("month")
                .map(|sig| physical_to_u64(&sig.value));
            let year = msg
                .decoded
                .signals
                .get("year")
                .map(|sig| physical_to_u64(&sig.value));

            if let (Some(ms), Some(s), Some(min), Some(h), Some(d), Some(mon), Some(y)) =
                (millisecond, second, minute, hour, day, month, year)
            {
                let full_year = if y < 100 { 2000 + y as i32 } else { y as i32 };
                // Construct a chrono::DateTime from the extracted values
                if let Some(dt) = chrono::NaiveDate::from_ymd_opt(full_year, mon as u32, d as u32)
                    .and_then(|date| {
                        date.and_hms_milli_opt(h as u32, min as u32, s as u32, ms as u32)
                    })
                {
                    let dt_utc = chrono::Utc.from_utc_datetime(&dt);

                    let current_year = chrono::Utc::now().year();
                    if dt_utc.year() < current_year - 2 || dt_utc.year() > current_year + 2 {
                        log::warn!(
                            "GPS message at {} ms has suspicious year value {}, skipping",
                            msg.timestamp,
                            dt_utc.year()
                        );
                        continue;
                    }

                    gps_points.push((msg.timestamp, dt_utc));
                } else {
                    log::error!(
                        "GPS message at {} ms has invalid date/time values, skipping",
                        msg.timestamp
                    );
                    continue;
                }
            } else {
                log::error!(
                    "GPS message at {} ms is missing some time signals, skipping",
                    msg.timestamp
                );
                continue;
            }
        }
    }

    if gps_points.is_empty() {
        // No GPS points found, can't correlate
        log::warn!("No GPS points found in chunk, cannot correlate timestamps");
        return CorrelationChunkResult::uncorrelated_new(chunk);
    }

    // Attempt to fit a line to the GPS points to find the correlation function
    let points: Vec<Point> = gps_points
        .iter()
        .map(|(log_ts, real_ts)| Point {
            x: *log_ts as f64,
            y: real_ts.timestamp_millis() as f64,
        })
        .collect();
    let (slope, intercept) = match linear_regression(&points) {
        Some(v) => v,
        None => {
            log::error!(
                "Failed to refit correlation line. Points count: {}",
                points.len()
            );
            return CorrelationChunkResult::uncorrelated_new(chunk);
        }
    };

    // Print debug info about the correlation quality
    let rms_error_ms = {
        let mse = points
            .iter()
            .map(|p| {
                let predicted = slope * p.x + intercept;
                let error = p.y - predicted;
                error * error
            })
            .sum::<f64>()
            / points.len() as f64;

        mse.sqrt()
    };
    log::info!(
        "GPS correlation successful: slope={:.9}, intercept_ms={:.3}, rms_error_ms={:.2}, points={}",
        slope,
        intercept,
        rms_error_ms,
        points.len()
    );

    // Log error (but continue) if slope is not ~REGRESSION_FALLBACK_SLOPE
    if (slope - REGRESSION_FALLBACK_SLOPE).abs() > 0.001 {
        log::error!(
            "GPS correlation slope ({:.9}) deviates from expected {:.3}.",
            slope,
            REGRESSION_FALLBACK_SLOPE,
        );
    }

    CorrelationChunkResult::correlated_new(
        chunk,
        CorrelationFunction {
            slope,
            intercept_ms: intercept,
        },
    )
}

struct Point {
    x: f64, // log timestamp ms
    y: f64, // unix timestamp ms
}

/// Least squares linear regression.
///
/// Note: if there is only 1 point, uses REGRESSION_FALLBACK_SLOPE
///
/// Fits:
/// y = slope * x + intercept
fn linear_regression(points: &[Point]) -> Option<(f64, f64)> {
    if points.is_empty() {
        return None;
    }

    // Fallback for single point: use a default slope and calculate intercept based on that point
    if points.len() == 1 {
        log::warn!(
            "Only one GPS point found, using fallback slope of {}",
            REGRESSION_FALLBACK_SLOPE
        );
        let p = &points[0];
        let slope = REGRESSION_FALLBACK_SLOPE;
        let intercept = p.y - slope * p.x;
        return Some((slope, intercept));
    }

    let n = points.len() as f64;

    let mut sum_x = 0.0;
    let mut sum_y = 0.0;
    let mut sum_xy = 0.0;
    let mut sum_x2 = 0.0;

    for p in points {
        sum_x += p.x;
        sum_y += p.y;
        sum_xy += p.x * p.y;
        sum_x2 += p.x * p.x;
    }

    let denom = n * sum_x2 - sum_x * sum_x;

    if denom.abs() < 1e-9 {
        return None;
    }

    let slope = (n * sum_xy - sum_x * sum_y) / denom;
    let intercept = (sum_y - slope * sum_x) / n;

    Some((slope, intercept))
}
