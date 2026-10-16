mod consts;
mod db;
mod state;

#[tokio::main]
async fn main() -> Result<(), db::Error> {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info")).init();

    let database = db::init_pool(consts::DATABASE_URL)?;
    let state = state::AppState { db: database };

    let app = axum::Router::new()
        .route("/", axum::routing::get(|| async { "daqcloud\n" }))
        .with_state(state);

    let listener = tokio::net::TcpListener::bind(consts::BIND_ADDR).await?;
    log::info!("daqcloud listening on http://{}", listener.local_addr()?);

    axum::serve(listener, app)
        .with_graceful_shutdown(async {
            if let Err(error) = tokio::signal::ctrl_c().await {
                log::error!("Could not listen for shutdown: {error}");
            }
        })
        .await?;
    Ok(())
}
