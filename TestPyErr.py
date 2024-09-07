#!/bin/sh
# -*- python -*-
""":"
exec python3 $0 ${1+"$@"}
"""

import time
from threading import Thread

class T(Thread):
    def __init__(self, n, m, fail = False):
        self.m = m
        self.n = n
        self.f = fail
        Thread.__init__(self)

    def start(self):
        Thread.start(self)

    def run(self):
        print('->thread {}'.format(self.n))
        if self.m:
            for n in range(0,self.m):
                print('{}: {}'.format(self.n, n))
                time.sleep(0.4)
            if self.f:
                wtf
        else:
            # forever
            n = 1
            while True:
                print('{}: {}'.format(self.n, n))
                time.sleep(0.4)
                n += 1
        print('<-thread {}'.format(self.n))

t1 = T('t1', 7, True)
t1.start()
t2 = T('t2', 13)
t2.start()
t3 = T('t3', 0)
t3.start()

for n in range(0,5):
    print('Number1 {}'.format(n))
    time.sleep(0.5)
