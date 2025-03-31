use anyhow::{anyhow, Context, Result};
use std::{collections::HashMap, sync::LazyLock};

#[derive(Debug, Default)]
pub struct PacketNames {
    clientbound: HashMap<u16, String>,
    serverbound: HashMap<u16, String>,
    tribulle_clientbound: HashMap<u16, String>,
    tribulle_serverbound: HashMap<u16, String>,
}

#[derive(Debug)]
pub enum PktNames {
    Serverbound,
    Clientbound,
    TribulleClientbound,
    TribulleServerbound,
}

impl PacketNames {
    pub fn from_json(value: &jzon::JsonValue) -> Result<Self> {
        Ok(Self {
            clientbound: Self::get_json_map(value, "clientbound")?,
            serverbound: Self::get_json_map(value, "serverbound")?,
            tribulle_clientbound: Self::get_json_map(value, "tribulle_clientbound")?,
            tribulle_serverbound: Self::get_json_map(value, "tribulle_serverbound")?,
        })
    }

    pub fn get(&self, category: &PktNames, code: u16) -> Option<&String> {
        match category {
            PktNames::Clientbound => &self.clientbound,
            PktNames::Serverbound => &self.serverbound,
            PktNames::TribulleClientbound => &self.tribulle_clientbound,
            PktNames::TribulleServerbound => &self.tribulle_serverbound,
        }
        .get(&code)
    }

    fn get_json_map(value: &jzon::JsonValue, name: &str) -> Result<HashMap<u16, String>> {
        Self::json_to_map(value.get(name).unwrap_or(&jzon::Null)).context(format!("Invalid {name}"))
    }
    fn json_to_map(value: &jzon::JsonValue) -> Result<HashMap<u16, String>> {
        value
            .as_object()
            .ok_or(anyhow!("Json entries should be objects"))?
            .iter()
            .map(|(key, value)| {
                let key = u16::from_str_radix(key, 16)
                    .context("Keys should be packet's code in hexadecimals")?;
                let value = value.as_str().ok_or(anyhow!("Value should be a string"))?;
                let mut name = String::with_capacity(value.len());
                for part in value.split(['_', ' ']) {
                    let mut it = part.chars();
                    name.extend(it.next().map(|c| c.to_ascii_uppercase()).into_iter());
                    name.extend(it.filter(|c| c.is_ascii_alphabetic()));
                }

                Ok((key, name))
            })
            .collect()
    }
}

macro_rules! include_packets {
    ($file:expr $(,)?) => {
        PacketNames::json_to_map(&jzon::parse(include_str!($file)).unwrap()).unwrap()
    };
}

pub static PKT_NAMES: LazyLock<PacketNames> = LazyLock::new(|| PacketNames {
    clientbound: include_packets!("../../packets/clientbound.json"),
    serverbound: include_packets!("../../packets/serverbound.json"),
    tribulle_clientbound: include_packets!("../../packets/tribulle_clientbound.json"),
    tribulle_serverbound: include_packets!("../../packets/tribulle_serverbound.json"),
});
