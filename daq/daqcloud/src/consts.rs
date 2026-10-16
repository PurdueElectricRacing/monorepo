use std::net;

pub const DATABASE_URL: &str = "daqcloud.db";
pub const BIND_ADDR: net::SocketAddr =
    net::SocketAddr::V4(net::SocketAddrV4::new(net::Ipv4Addr::UNSPECIFIED, 3000));
