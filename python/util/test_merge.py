__author__ = 'dan'
import discodb
a = discodb.DiscoDBConstructor()
a.add("k","v")
o = a.finalize(unique_items=True)
print [k for k in o.keys()]
print [v for v in o.values()]
with open("/tmp/qfd1", 'wb') as f:
    o.dump(f)
with open("/tmp/qfd1", 'rb') as f:
    b = discodb.DiscoDB.load(f)
print [k for k in b.keys()]
print [v for v in b.values()]
type(b)
c = discodb.DiscoDBConstructor()
c.add("k","2")
c.add("k","3")
c.add("k","2")
c.merge(b, False)
oo = c.finalize(unique_items=True)
print [k for k in oo.keys()]
print [v for v in oo.values()]
with open("/tmp/qfd2", 'wb') as f:
    oo.dump(f)
d = discodb.DiscoDBConstructor()
with open("/tmp/qfd2", 'rb') as f:
    e = discodb.DiscoDB.load(f)
d.merge_with_explicit_value(e, "new_value_for_all_keys")
oo2 = d.finalize(unique_items=True)
[v for v in oo2.values()]