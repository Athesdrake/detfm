use crate::{detfm::pktnames::PktNames, renamer::Counters};
use rabc::abc::Trait;

pub trait Formatter: std::fmt::Debug {
    fn traits(&self, ctrait: &Trait, counters: &mut Counters) -> String;
    fn classes(&self, counter: u32) -> String;
    fn errors(&self, counter: u32) -> String;
    fn symbols(&self, id: u16) -> String;
    fn packets(&self, side: PktNames, pkt_id: u16, name: String) -> String;
    fn subhandler(&self, category: u8) -> String;
    fn unknown_packet(&self, counter: u32) -> String;
}
