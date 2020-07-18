#!/usr/bin/python

import sys

outfile = open('data.json', 'w')
infile = open('timing_drawer.txt','r')

firstline = infile.readline()

num_columns = len(firstline.split(";"))

# Skip the time field
field_num = 1

while (field_num < num_columns):
    var = firstline.split(";")
    var2 = var[field_num].split("=")
    field_name = var2[0]
    infile.seek(0)
    print("   {name: '" + field_name + "', wave: '", file=outfile, end='')
    prev_val = ''
    if (field_name == "DATA"):
        for line in infile:
            value_pair = line.split(";")[field_num]
            z = value_pair.split("=")[1].strip().replace(".","")
            if(z == prev_val):
                out = '.'
                prev_val = z
            elif (z == '00'):
                out = '0'
            else:
                out = '1'
            print(out, file=outfile, end='')
            prev_val = z
        print("', data: [", file=outfile, end='')
        infile.seek(0)
        for line in infile:
            value_pair = line.split(";")[field_num]
            z = value_pair.split("=")[1].strip().replace(".","")
            if(z == prev_val):
                continue
            print("'" + z + "', ", file=outfile, end='')
            prev_val = z
        print("]},", file=outfile)
    else:
        for line in infile:
            value_pair = line.split(";")[field_num]
            z = value_pair.split("=")[1]
            if(z == prev_val):
                z = '.'
            else:
                prev_val = z
            print(z, file=outfile, end='')

    print("'},", file=outfile)

    field_num = field_num + 1




outfile.close()


