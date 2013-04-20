from discodb import DiscoDB
from itertools import permutations, islice, imap, izip_longest
from time import time
from random import sample
from csv import DictWriter

NUM_VALUES = int(1e6)
WORD_LEN = 64

def items():
    chars = map(chr, range(1, WORD_LEN + 1))
    v = imap(lambda x: ''.join(x), permutations(chars))
    return izip_longest([], islice(v, NUM_VALUES), fillvalue='a')

def timed(f, n=10):
    def run():
        s = time()
        f()
        return time() - s
    return min(run() for i in range(n)) * 1000

def samples(db):
    values = list(db.unique_values())
    for i in range(0, 110, 10):
        s = frozenset(sample(values, int(NUM_VALUES * (i / 100.))))
        v = db.make_view(s)
        yield v, s

def test(db):
    for v, s in samples(db):
        if list(db.query('a', view=v)) != [x for x in db['a'] if x in s]:
            raise Exception("no match: %d" % i)
    print "all ok!"

def bmark(db):
    for i, (v, s) in enumerate(samples(db)):
        yield {"sample-size": i * 10,
               "list-baseline": timed(lambda: sum(1 for _ in db['a'])),
               "list-if": timed(lambda: sum(1 for x in db['a'] if x in s)),
               "list-view": timed(lambda: sum(1 for x in db.query('a', view=v))),
               "count-baseline": timed(lambda: len(db['a'])),
               "count-if": timed(lambda: sum(1 for x in db['a'] if x in s)),
               "count-view": timed(lambda: len(db.query('a', view=v)))}

db = DiscoDB(items())
test(db)
print "tests pass"
rows = list(bmark(db))
f = open('bmark.csv', 'w')
csv = DictWriter(f, sorted(rows[0].keys(), reverse=True))
csv.writeheader()
csv.writerows(rows)
f.close()

