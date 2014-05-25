#pragma once

#include "interfaces/ModePluginDockWidget.h"
#include "SurfaceMeshPlugins.h"
#include "GraphManager.h"
#include "FoldManager.h"
#include <QColorDialog>

class FdWidget;

class FdPlugin : public SurfaceMeshModePlugin
{
	Q_OBJECT
	Q_INTERFACES(ModePlugin)

	// Plugin interfaces
    QIcon icon(){ return QIcon(":/images/icon.png"); }
	void create();
	void destroy();
	void decorate();

	void drawWithNames();
	bool postSelection(const QPoint& point);

public:
	FdWidget* widget;
	GraphManager* g_manager;
	FoldManager* f_manager;

	bool drawAABB;
	bool drawCuboid;
	bool drawScaffold;
	bool drawMesh;
	bool drawFolded;
	bool drawKeyframe;

	QColorDialog* qColorDialog;
public:
    FdPlugin();
	
	// helpers
	FdGraph* activeScaffold();
	QVector<FdNode*> selectedFdNodes();

public slots:
	// to graph manager
	void resetMesh();

	// show options
	void showAABB(int state);
	void showCuboid(int state);
	void showScaffold(int state);
	void showMesh(int state);
	void showFolded(int state);
	void showKeyframe(int state);

	// scene and message
	void updateScene();
	void resetScene();
	void showStatus(QString msg);

	// export
	void exportCurrent();

	// color dialog
	void showColorDialog();
	void updateSelNodesColor(QColor c);

	void test1();
	void test2();
};


