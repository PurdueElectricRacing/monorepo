pub mod schema;

use std::{error, time};

use diesel::connection::SimpleConnection as _;
use diesel_migrations::MigrationHarness as _;

pub type Error = Box<dyn error::Error + Send + Sync>;
pub type DbPool = diesel::r2d2::Pool<diesel::r2d2::ConnectionManager<diesel::SqliteConnection>>;

const MIGRATIONS: diesel_migrations::EmbeddedMigrations =
    diesel_migrations::embed_migrations!("migrations");

const POOL_SIZE: u32 = 4;
const CONNECTION_TIMEOUT: time::Duration = time::Duration::from_secs(5);

#[derive(Debug)]
struct SqliteSettings;

impl diesel::r2d2::CustomizeConnection<diesel::SqliteConnection, diesel::r2d2::Error>
    for SqliteSettings
{
    fn on_acquire(&self, conn: &mut diesel::SqliteConnection) -> Result<(), diesel::r2d2::Error> {
        // These settings apply to each connection, including pool replacements.
        conn.batch_execute("PRAGMA busy_timeout = 5000; PRAGMA foreign_keys = ON;")
            .map_err(diesel::r2d2::Error::QueryError)
    }
}

/// Open a file-backed SQLite database and migrate it before serving requests.
pub fn init_pool(database_url: &str) -> Result<DbPool, Error> {
    let manager = diesel::r2d2::ConnectionManager::<diesel::SqliteConnection>::new(database_url);
    let pool = diesel::r2d2::Pool::builder()
        .max_size(POOL_SIZE)
        .connection_timeout(CONNECTION_TIMEOUT)
        .connection_customizer(Box::new(SqliteSettings))
        .build(manager)?;

    {
        let mut conn = pool.get()?;
        // WAL is persistent for this database file.
        // Allows readers during writes.
        conn.batch_execute("PRAGMA journal_mode = WAL;")?;
        conn.run_pending_migrations(MIGRATIONS)?;
    }

    Ok(pool)
}
