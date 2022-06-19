
file = open("untitled.obj", "r")
lines = file.readlines()

count = 0
vs = []
for line in lines:
    if line.startswith("v "):
        vs.append(line)
        count = count + 1

output = ""
i = 0
for v in vs:
    output += v + "vt " + "{:.5f}".format(i/count) + "\n"
    i = i+1
    if i % int(count/10) == 0:
        output += "l\n"
    
output += "l\n"
file.close()
file = open("test.obj", "w")
file.write(output)
file.close()