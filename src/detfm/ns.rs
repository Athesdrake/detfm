#[derive(Debug)]
pub struct NsNames {
    /// packets
    pub pkt: u32,
    /// packets.serverbound
    pub spkt: u32,
    /// packets.clientbound
    pub cpkt: u32,
    /// packets.tribulle
    pub tpkt: u32,
    /// packets.tribulle.serverbound
    pub tspkt: u32,
    /// packets.tribulle.clientbound
    pub tcpkt: u32,
    pub slot: u32,
}
