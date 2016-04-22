import sys
import os
import json
import datetime

superdict = {'MOVE' : {},
             'STOP' : {},
             'TURN' : {},
             'IMAGE' : {},
             'GPS' : {},
             'DGPS' : {},
             'LASERS' : {}}


for line in sys.stdin:
    split_line = line.split(':')
    if(len(split_line) != 2):
        continue

    value_name = split_line[0]
    value = float(split_line[1])

    for subdict_name, subdict in superdict.items():
        if (subdict_name in value_name):
            entry_name = subdict_name + '_' + str(len(subdict))
            subdict[entry_name] = value

# print contents of constructed super dict
for key, subdict in superdict.items():
    print key
    for item_name, item in subdict.items():
        print "\t%s : %r" % (item_name, item)

    print ""

path = "./test_data"
if not os.path.exists(path):
    os.makedirs(path)

now = datetime.datetime.now().isoformat()
filename = "output_" + now + ".json"
f = open(os.path.join(path, filename), 'w')
f.write(json.dumps(superdict))
f.close()
