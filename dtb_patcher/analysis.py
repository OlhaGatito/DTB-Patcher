import re
from .models import Change

RULES = {
 "input": r"joypad|gamepad|joystick|gpio.keys|buttons?|input|adc.keys",
 "analog": r"rocker|joystick|adc|io.channels|analog",
 "audio": r"audio|sound|codec|i2s|spdif|simple.audio",
 "display": r"display|panel|dsi|mipi|lcd|hdmi|rgb",
 "backlight": r"backlight|pwm.backlight",
 "battery": r"battery|charger|fuel.gauge",
 "power": r"regulator|power|vbus|usb",
 "gpio": r"gpio|pinctrl|pinctrl-",
}

def category(path, prop=""):
    s=(path+" "+prop).lower()
    for k,rx in RULES.items():
        if re.search(rx,s): return k
    return "other"

def compare(a,b):
    changes=[]
    am={n.path:n for n in a.root.walk()}; bm={n.path:n for n in b.root.walk()}
    for p,n in am.items():
        if p=="/": continue
        if p not in bm:
            changes.append(Change(p,"add-node",f"Doador possui nó ausente no Receptor: {n.name}",category(p)))
            continue
        for pn,pr in n.properties.items():
            br=bm[p].properties.get(pn)
            if br is None: changes.append(Change(p+"/"+pn,"add-property",f"Adicionar {pn}",category(p,pn),"high"))
            elif pr.value != br.value: changes.append(Change(p+"/"+pn,"change-property",f"{br.value} -> {pr.value}",category(p,pn),"high"))
    return changes
