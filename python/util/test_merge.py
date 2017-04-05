__author__ = 'dan'
import discodb
from time import sleep
a = discodb.DiscoDBConstructor()
a.add("k","v")
a.add("db1","a value")
for x in xrange(200000):
    a.add("special", str(x))
o = a.finalize(unique_items=True)
print "DB1 Keys and Values"
print [k for k in o.keys()]
print [v for v in o.values()]
with open("/tmp/qfd1", 'wb') as f:
    o.dump(f)
with open("/tmp/qfd1", 'rb') as f:
    b = discodb.DiscoDB.load(f)

print "DB1 Keys and Values read from disk"
print [k for k in b.keys()]
print [v for v in b.values()]
type(b)

c = discodb.DiscoDBConstructor()
c.add("k","2")
c.add("k","3")
c.add("db2","another value")
c.add("k","2")
for x in xrange(200000):
    c.add("special", str(x))
c.merge(b, False)
oo = c.finalize(unique_items=True)

print "DB2 Keys and Values"
print [k for k in oo.keys()]
print [v for v in oo.values()]
with open("/tmp/qfd2", 'wb') as f:
    oo.dump(f)
d = discodb.DiscoDBConstructor()
with open("/tmp/qfd2", 'rb') as f:
    e = discodb.DiscoDB.load(f)


#d.merge_with_explicit_value(e, "new_value_for_all_keys") #one of these must be commented
d.merge(e, False) #one of these must be commented

oo2 = d.finalize(unique_items=True)
print "testing the final merge"
print "values before reload "
[v for v in oo2.values()]

with open("/tmp/qfdfinal", 'wb') as f:
    oo2.dump(f)
with open("/tmp/qfdfinal", 'rb') as f:
    oofinal = discodb.DiscoDB.load(f)

print "key/values after reload "
for k in oofinal.keys():
    for v in oofinal.query(discodb.Q.parse(k)):
        print k, "/", v

sleep(300)