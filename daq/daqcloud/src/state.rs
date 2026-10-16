use crate::db;

#[derive(Clone)]
pub struct AppState {
    #[allow(dead_code)]
    pub db: db::DbPool,
}
