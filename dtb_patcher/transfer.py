from copy import deepcopy
from .models import Change, Node, Property
from .analysis import category

class TransferError(RuntimeError): pass

def find(root,path):
    if path=="/": return root
    cur=root
    for part in [x for x in path.split("/") if x]:
        cur=cur.children.get(part)
        if cur is None: return None
    return cur

def refs(value):
    return [value[i+1:] for i in range(len(value)) if value[i]=="&" and i+1<len(value) and value[i+1] not in "{"]

def apply(doc, donor, selected):
    result=deepcopy(doc)
    changes=[]
    for item in selected:
        path=item.path
        if item.kind=="add-node":
            src=find(donor.root,path)
            parent=find(result.root,"/" + "/".join(path.strip("/").split("/")[:-1])) or result.root
            if src and parent:
                parent.children[src.name]=deepcopy(src)
                changes.append(item)
            else: raise TransferError(f"Nó não pode ser inserido com segurança: {path}")
        else:
            nodepath,prop=path.rsplit("/",1)
            dst=find(result.root,nodepath); src=find(donor.root,nodepath)
            if not dst or not src or prop not in src.properties:
                raise TransferError(f"Propriedade não resolvida: {path}")
            dst.properties[prop]=deepcopy(src.properties[prop]); changes.append(item)
    return result, changes
