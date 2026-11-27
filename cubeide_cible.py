#!/usr/bin/env python3
"""
cubeide_cible.py - Remet la cible MCU dans les .cproject des projets STM32CubeIDE du dépôt.

Problème : à l'import d'un projet Makefile, CubeIDE régénère souvent le .cproject et laisse vides
les options « MCU », « CPU », « Core », « Floating-point unit » et « Floating-point ABI ». La
compilation passe (le Makefile donne ses propres options), mais le débogage et le flash depuis
CubeIDE échouent avec « Not able to get mcu target ». La compilation parallèle peut aussi être
désactivée.

Ce script remplit ces options sans toucher au reste du fichier, en gardant ses fins de ligne :
    STM32L476RGTx  pour les projets NUCLEO-L476RG (STM32WB55 si un projet NUCLEO-WB55 est ajouté)
    CPU 0, Core 0, FPU fpv4-sp-d16, Float ABI hard, compilation parallèle (jobs optimaux)
    script de liaison désigné par ${ProjDirPath}/<fichier>.ld au lieu de ${workspace_loc:/${ProjName}/...}
      (projets L476 : « Cannot find the specified linker script » après un réimport, et chemin
      \\wsl.localhost tronqué par make)

Le projet ST de référence NUCLEO_L476RG_TOF_VL53L5A1 n'est jamais modifié.

Utilisation (depuis WSL ou Windows, à la racine du dépôt) :
    1. Dans CubeIDE : clic droit sur le projet > Close Project (sinon CubeIDE réécrit le fichier).
    2. python3 cubeide_cible.py              # tous les projets du dépôt
       python3 cubeide_cible.py sentinelle/NUCLEO_L476RG_TOF_SENTINELLE
    3. Dans CubeIDE : clic droit > Open Project, puis déboguer.

    python3 cubeide_cible.py --check         # vérifie seulement ; code 1 si un projet est à corriger
"""
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
REFERENCE = "NUCLEO_L476RG_TOF_VL53L5A1"     # projet ST de référence : lecture seule
OPT = "com.st.stm32cube.ide.mcu.gnu.managedbuild.option."
FPU = OPT + "fpu.value.fpv4-sp-d16"
ABI = OPT + "floatabi.value.hard"


def mcu_for(path):
    """Puce du projet, d'après les définitions du Makefile ou le nom du dossier."""
    proj = os.path.dirname(os.path.abspath(path))
    text = ""
    mk = os.path.join(proj, "Makefile")
    if os.path.exists(mk):
        text = open(mk, encoding="utf-8", errors="replace").read()
    name = os.path.basename(proj).upper()
    if "MB1293" in name or "MB1293" in text:          # dongle USB P-NUCLEO-WB55 (boîtier UFQFPN48)
        return "STM32WB55CGUx"
    if "STM32WB55" in text or "WB55" in name or "NUCLEOWB" in name:
        return "STM32WB55RGVx"
    if "STM32L476" in text or "L476" in name:
        return "STM32L476RGTx"
    return None


# option (fin du superClass) -> (valeur, type)
def wanted(mcu):
    return {
        "target_mcu": (mcu, "string"),
        "target_cpuid": ("0", "string"),
        "target_coreid": ("0", "string"),
        "fpu": (FPU, "enumerated"),
        "floatabi": (ABI, "enumerated"),
    }


def fix_text(text, mcu, has_makefile=True):
    """Retourne (nouveau texte, liste des corrections). Lève ValueError si le fichier n'utilise pas
    la chaîne d'outils « MCU ARM GCC » de ST, ou si un projet sans Makefile (compilation gérée par
    CubeIDE) a été transformé en projet Makefile : rien à remplir, il faut réimporter le projet."""
    if 'name="MCU ARM GCC"' not in text:
        raise ValueError("pas de chaîne d'outils « MCU ARM GCC » (configuration générique « No ToolChain » ?)")
    if not has_makefile and 'managedBuildOn="true"' not in text:
        raise ValueError("projet sans Makefile transformé en projet Makefile (configuration « Default », "
                         "« make: No rule to make target 'all' ») : importé par « Makefile Project with "
                         "Existing Code » ?")
    changes = []
    for key, (value, vtype) in wanted(mcu).items():
        pat = re.compile(r'<option\b[^>]*superClass="' + re.escape(OPT + key) + r'"[^>]*?/>')
        m = pat.search(text)
        if not m:
            raise ValueError(f"option {key} absente")
        tag = m.group(0)
        if f'value="{value}"' in tag:
            continue
        new = re.sub(r'\s(value|valueType)="[^"]*"', "", tag)
        new = new[:-2].rstrip() + f' value="{value}" valueType="{vtype}"/>'
        text = text[:m.start()] + new + text[m.end():]
        changes.append(f"{key} = {value.rsplit('.', 1)[-1] if key in ('fpu', 'floatabi') else value}")
    def script_fix(m):
        value = m.group(2)
        if "${workspace_loc:" not in value:
            return m.group(0)
        name = value.rstrip("}").rsplit("/", 1)[-1]
        changes.append(f"script de liaison = ${{ProjDirPath}}/{name}")
        return m.group(1) + "${ProjDirPath}/" + name + m.group(3)
    text = re.sub(r'(<option\b[^>]*superClass="' + re.escape("com.st.stm32cube.ide.mcu.gnu.managedbuild.tool.c.linker.option.script") + r'"[^>]*?value=")'
                  r'([^"]*)(")', script_fix, text)
    m = re.search(r'<builder\b[^>]*/>', text)
    if m:
        tag = m.group(0)
        new = tag
        for attr, value in (("parallelBuildOn", "true"), ("parallelizationNumber", "optimal")):
            if f'{attr}="{value}"' not in new:
                new = re.sub(r'\s' + attr + r'="[^"]*"', "", new)
                new = new[:-2].rstrip() + f' {attr}="{value}"/>'
        if new != tag:
            text = text[:m.start()] + new + text[m.end():]
            changes.append("compilation parallèle")
    return text, changes


def projects(args):
    if args:
        out = []
        for a in args:
            p = a if a.endswith(".cproject") else os.path.join(a, ".cproject")
            out.append(p)
        return out
    found = glob.glob(os.path.join(ROOT, "*", ".cproject")) + glob.glob(os.path.join(ROOT, "*", "*", ".cproject"))
    return sorted(p for p in found if "/build/" not in p.replace("\\", "/")
                  and REFERENCE not in p.replace("\\", "/").split("/"))


def main():
    check = "--check" in sys.argv
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    bad = 0          # à corriger par ce script
    broken = 0       # à réimporter (ce script ne peut rien)
    for path in projects(args):
        rel = os.path.relpath(path, ROOT)
        mcu = mcu_for(path)
        if mcu is None:
            print(f"  ?     {rel} : puce inconnue, ignoré")
            continue
        with open(path, "rb") as f:
            raw = f.read()
        crlf = b"\r\n" in raw
        text = raw.decode("utf-8").replace("\r\n", "\n")
        try:
            new, changes = fix_text(text, mcu, os.path.exists(os.path.join(os.path.dirname(path), "Makefile")))
        except ValueError as e:
            broken += 1
            print(f"  ECHEC {rel} : {e}\n"
                  f"        → Delete dans CubeIDE (sans effacer les fichiers), puis : git restore {rel}\n"
                  f"          et réimporter par File > Import > Existing Projects into Workspace")
            continue
        if not changes:
            print(f"  ok    {rel} ({mcu})")
            continue
        bad += 1
        if check:
            print(f"  A CORRIGER {rel} : {', '.join(changes)}")
            continue
        if crlf:
            new = new.replace("\n", "\r\n")
        with open(path, "wb") as f:
            f.write(new.encode("utf-8"))
        print(f"  CORRIGE {rel} : {', '.join(changes)}")
    if check and bad:
        print("→ Fermer le projet dans CubeIDE (Close Project), lancer : python3 cubeide_cible.py, puis le rouvrir.")
    sys.exit(1 if broken or (check and bad) else 0)


if __name__ == "__main__":
    main()
