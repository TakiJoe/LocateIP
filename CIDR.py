#!/usr/bin/env python
# -*- coding: utf-8 -*-  

from netaddr import *

f_file = open('17mon.txt', 'r')
t_file = open('CIDRs.txt', 'a')

for line in f_file:
    line = line.split()
    a = line[0]
    b = line[1]
    c = " ".join(line[2:])
    ip = IPRange(a,b)
    for i in ip.cidrs():
        d = str(i)+" "+c+"\r\n"
        t_file.write(d)
f_file.close()
t_file.close()
