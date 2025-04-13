use anyhow::{anyhow, ensure, Result};
use rabc::abc::{Class, ConstantPool, Exception, Method, Trait};

pub trait PoolRenamer {
    fn get_mn_name(&self, mn: u32) -> Result<u32>;

    fn replace_string(&mut self, old: &str, new: &str);
    fn rename_str(&mut self, index: u32, name: String);
    fn rename_multiname(&mut self, index: u32, name: String) -> Result<()>;
}

impl PoolRenamer for ConstantPool {
    fn get_mn_name(&self, mn: u32) -> Result<u32> {
        self.multinames[mn as usize]
            .get_name_index()
            .ok_or_else(|| anyhow!("Multiname {mn:?} does not have a name!"))
    }

    fn replace_string(&mut self, old: &str, new: &str) {
        for s in self.strings.iter_mut() {
            if s == old {
                *s = new.to_owned();
            }
        }
    }

    fn rename_str(&mut self, index: u32, name: String) {
        self.strings[index as usize] = name;
    }

    fn rename_multiname(&mut self, index: u32, name: String) -> Result<()> {
        self.rename_str(self.get_mn_name(index)?, name);
        Ok(())
    }
}

pub trait Rename {
    fn rename(&self, cpool: &mut ConstantPool, name: String) -> Result<()>;
    fn rename_str(&self, cpool: &mut ConstantPool, name: &str) -> Result<()> {
        self.rename(cpool, name.to_owned())
    }
}
pub trait RenameSuper {
    fn rename_super(&self, cpool: &mut ConstantPool, name: String) -> Result<()>;
}

impl Rename for Class {
    fn rename(&self, cpool: &mut ConstantPool, name: String) -> Result<()> {
        cpool.rename_multiname(self.name, name)
    }
}
impl RenameSuper for Class {
    fn rename_super(&self, cpool: &mut ConstantPool, name: String) -> Result<()> {
        cpool.rename_multiname(self.super_name, name)
    }
}

impl Rename for Trait {
    fn rename(&self, cpool: &mut ConstantPool, name: String) -> Result<()> {
        let mn = self.name();
        ensure!(mn != 0, "Cannot rename an unnamed trait.");
        cpool.rename_multiname(mn, name)
    }
}

impl Rename for Exception {
    fn rename(&self, cpool: &mut ConstantPool, name: String) -> Result<()> {
        ensure!(self.var_name != 0, "Cannot rename an unnamed error.");
        cpool.rename_multiname(self.var_name, name)
    }
}

impl Rename for Method {
    fn rename(&self, cpool: &mut ConstantPool, name: String) -> Result<()> {
        ensure!(self.name != 0, "Cannot rename an unnamed method.");
        cpool.rename_str(self.name, name);
        Ok(())
    }
}
