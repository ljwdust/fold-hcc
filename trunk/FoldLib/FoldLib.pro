load($$[STARLAB])
load($$[SURFACEMESH])


TEMPLATE = lib
CONFIG += staticlib

# Build flag
CONFIG(debug, debug|release) {
    CFG = debug
} else {
    CFG = release
}

# Utility library
LIBS += -L$$PWD/../UtilityLib/$$CFG/lib -lUtilityLib
INCLUDEPATH += ../UtilityLib

# Geometry library
LIBS += -L$$PWD/../GeometryLib/$$CFG/lib -lGeometryLib
INCLUDEPATH += ../GeometryLib

# Structure library
LIBS += -L$$PWD/../StructureLib/$$CFG/lib -lStructureLib
INCLUDEPATH += ../StructureLib

# Mesh Utility library
LIBS += -L$$PWD/../MeshUtilityLib/$$CFG/lib -lMeshUtilityLib
INCLUDEPATH += ../MeshUtilityLib

# Library name and destination
TARGET = FoldLib
DESTDIR = $$PWD/$$CFG/lib

HEADERS += \
    PatchNode.h \
    RodNode.h \
    GraphManager.h \
    FdGraph.h \
    FdLink.h \
    FdNode.h \
    DcGraph.h \
    FoldManager.h \
    FdUtility.h \
    LayerGraph.h \
    SandwichLayer.h \
    PizzaLayer.h \
    DependGraph.h \
    PizzaChain.h \
    SandwichChain.h \
    ChainGraph.h \
    Hinge.h

SOURCES += \
    PatchNode.cpp \
    RodNode.cpp \
    GraphManager.cpp \
    FdGraph.cpp \
    FdLink.cpp \
    FdNode.cpp \
    DcGraph.cpp \
    FoldManager.cpp \
    FdUtility.cpp \
    LayerGraph.cpp \
    SandwichLayer.cpp \
    PizzaLayer.cpp \
    DependGraph.cpp \
    PizzaChain.cpp \
    SandwichChain.cpp \
    ChainGraph.cpp \
    Hinge.cpp
