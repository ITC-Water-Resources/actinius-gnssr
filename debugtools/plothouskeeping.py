#!/usr/bin/python

import sys
import lz4.frame
import json
from datetime import datetime,timedelta
import math
import pandas as pd
import matplotlib.pyplot as plt

files=sorted(sys.argv[1:])

def extract_jsonheader(data):
    start=data.find('{')
    bracktrack=1 #>0 means json block is open
    for i,charac in enumerate(data[start+1:]):
        if charac == '{':
            bracktrack+=1
        elif charac == '}':
            bracktrack-=1

        if bracktrack == 0:
            return json.loads(data[start:i+start+2])


timetags=[]
mvolts=[]

for f in files:
    print(f"working on {f}")
    fdate=datetime.strptime(f[-19:-6],'%Y-%m-%d_%H')
    with lz4.frame.open(f, mode='rt') as fp:
        data = fp.read()
        header=extract_jsonheader(data)
        uptime=timedelta(hours=int(header['uptime']))
        for i in range(-int(uptime.seconds/3600),0):
            timetags.append(fdate+timedelta(hours=i))
            idx=i+fdate.hour
            if idx < 0:
                idx+=24
            mvolts.append(header['battery_mvolt'][idx])
            
        timetags.append(fdate)
        mvolts.append(header['battery_mvolt'][fdate.hour])


df=pd.DataFrame(dict(time=timetags,mvolts=mvolts)).set_index('time')

fig, ax = plt.subplots()
df.plot(ax=ax)
plt.show()



