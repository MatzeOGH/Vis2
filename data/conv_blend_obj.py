
file = open("shroom_blender.obj", "r")
lines = file.readlines()

def combineToPolyLines(prim):
    newPrimArray = []
    while len(prim) > 0:
        tmpPrim = prim.pop(0)
        madeChanges = True
        while madeChanges:
            madeChanges = False
            for line in prim:
                if tmpPrim[0] == line[len(line)-1]:
                    #line is a predecessor
                    tmpPrim = line[0:len(line)-1] + tmpPrim
                    prim.remove(line)
                    madeChanges = True
                if tmpPrim[len(tmpPrim) - 1] == line[0]:
                    #line is a successor
                    tmpPrim = tmpPrim + line[1:]
                    prim.remove(line)
                    madeChanges = True
        newPrimArray.append(tmpPrim)
    return newPrimArray

count = 0
vertices = []
prim = []
for line in lines:
    ss = line.strip().split(" ")
    if ss[0] == "v":
        vertices.append([float(ss[1]), float(ss[2]), float(ss[3])])
        count = count + 1
    elif ss[0] == "l":
        prim.append([int(ss[1])-1, int(ss[2])-1])

polys = combineToPolyLines(prim)

output = ""
for pline in polys:
    for vId in pline:
        vertex = vertices[vId]
        output += f"v {vertex[0]:.8f} {vertex[1]:.8f} {vertex[2]:.8f}\n"
        output += f"vt {vertex[2]}\n"
    output += "l\n"

file.close()
file = open("shroom.obj", "w")
file.write(output)
file.close()