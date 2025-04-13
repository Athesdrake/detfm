import json
from pathlib import Path
from sources import athesdrake, friedkeenan


def remove_unknown(classes: dict[str, str]):
    classes.pop("ffff", None)  # remove ExtensionWrapper packet
    for key in list(classes.keys()):
        if classes[key].startswith("Unknown") or "?" in classes[key]:
            del classes[key]

    return classes


def main():
    for side, bound in friedkeenan.FILES:
        filename = friedkeenan.get_filename(side, bound)
        classes = remove_unknown(athesdrake.get_packets(filename))
        classes.update(remove_unknown(friedkeenan.get_packets(side, bound)))

        with open(Path(__file__).parent / filename, "w") as f:
            json.dump(classes, f, indent=4, sort_keys=True)


if __name__ == "__main__":
    main()
