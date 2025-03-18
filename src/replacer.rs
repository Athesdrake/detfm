pub trait ReplaceAll<V> {
    fn replace_all(&mut self, old: &V, new: V) -> ();
}

impl<T: PartialEq<V> + From<V>, V: Clone> ReplaceAll<V> for Vec<T> {
    fn replace_all(&mut self, old: &V, new: V) -> () {
        for s in self.iter_mut() {
            if s == old {
                *s = new.clone().into();
            }
        }
    }
}
