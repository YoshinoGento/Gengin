"""Build the slatted harvest crate and split the hanging tomatoes without remeshing."""
from generate_farm_arrival import Mesh, OUT, save


def basket():
    shell, trim = Mesh(), Mesh()
    # Floor contact and end-board height are also used by the harvest landing pose.
    for z in (-.24, 0., .24):
        trim.box(0,.045,z,.53,.030,.114,0,"floorboard")
    for z in (-.342,.342):
        shell.box(0,.115,z,.50,.04,.018,0,"lower_board")
        trim.box(0,.221,z,.50,.047,.018,0,"upper_board")
    for x in (-.515,.515):
        shell.box(x,.130,0,.015,.055,.36,0,"end_board")
        # Leave a through-hole, not a painted mark: both handholds stay open from either side.
        for z in (-.249,.249):
            shell.box(x,.235,z,.015,.05,.111,0,"handhold_cheek")
        trim.box(x,.315,0,.015,.030,.36,0,"grip")
        for z in (-.297,.297):
            shell.box(.476 if x>0 else -.476,.210,z,.024,.135,.027,0,"corner_cleat")
    save(shell,"harvest_basket",True)
    save(trim,"harvest_basket_rim",True)


def tomatoes():
    lines=(OUT.parent/"crop_tomato.obj").read_text(encoding="ascii").splitlines()
    vertices=[tuple(map(float,row.split()[1:])) for row in lines if row.startswith("v ")]
    faces=[[tuple(map(int,v.split("/"))) for v in row.split()[1:]] for row in lines if row.startswith("f ")]
    parent=list(range(len(vertices)+1))
    def find(i):
        while i!=parent[i]:
            parent[i]=parent[parent[i]]
            i=parent[i]
        return i
    for f in faces:
        for k in (1,2):
            parent[find(f[k][0])]=find(f[0][0])
    groups={}
    for f in faces:
        groups.setdefault(find(f[0][0]),[]).append(f)
    if len(groups)!=2:
        raise ValueError("Expected two connected fruits in the field tomato mesh")
    normals=[row for row in lines if row.startswith("vn ")]
    uvs=[row for row in lines if row.startswith("vt ")]
    for i,group in enumerate(sorted(groups.values(),key=lambda g: -sum(vertices[f[0][0]-1][0] for f in g)/len(g))):
        # Grip-local centre in the original OBJ frame (mirrored X); see the field cultivar.
        x,y,z=[(-.171,.409,.045),(.202,.540,-.034)][i]
        pivot=(-x/.42,(y-.22)/.60,z/.42)
        used=sorted({v[0] for f in group for v in f})
        used_normals=sorted({v[2] for f in group for v in f})
        vi={v:j+1 for j,v in enumerate(used)}
        ni={v:j+1 for j,v in enumerate(used_normals)}
        output=["s off",*uvs]
        for idx in used:
            output.append("v "+" ".join(f"{v-p:.8f}" for v,p in zip(vertices[idx-1],pivot)))
        output.extend(normals[v-1] for v in used_normals)
        output.extend("f "+" ".join(f"{vi[v]}/{t}/{ni[n]}" for v,t,n in f) for f in group)
        (OUT/f"picked_tomato_{i}.obj").write_text("\n".join(output)+"\n",encoding="ascii")


if __name__=="__main__":
    basket()
    tomatoes()
