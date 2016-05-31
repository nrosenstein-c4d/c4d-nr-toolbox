+++
title = "Modeling Commands"
+++

## Explode Segments

The **Explode Segments** command is basically the Cinema 4D **Polygon Groups
to Objects** command, but many times faster. To use it, select a single Polygon
Object and run the command. The command employs an efficient algorithm that is
easily 20x faster than the native Cinema 4D command and it supports

- UVW
- Vertex Weights
- Vertex Color (R18+)

![Example](img/explode.jpg)

## Resolve Duplicates

The **Resolve Duplicates** command replaces duplicate Polygon Objects in your
scene with Cinema 4D **Render Instances**.

!!! note "Disclaimer"

    The larger and complex the objects for which the command is run, the more
    imprecise the results can be and in certain situations, seemingly duplicate
    objects can not be detected to be the same. Also keep in mind that the
    command only works on topologically equal objects with the same (relative)
    point coordinates.

    Only Polygon Objects can be replaced with this command.

When you run this command, it will ask you whether you want to enable undoing
the operation. Choose **No** for rather large scenes. The number of actions
that would need to be undone can be quite high and eventually freeze Cinema 4D
for a long time.

**Known Issues**

- Objects with the same topological properties but a shear
  transformation applied might not be resolved correctly.

![Example](img/resolve-duplicates.jpg)
