#pragma once
#include <QObject>

#include "UtilityGlobal.h"
#include "FdGraph.h"

class GraphManager : public QObject
{
	Q_OBJECT

public:
    GraphManager();
	SurfaceMeshModel* entireMesh;
	FdGraph* scaffold;

private:
	QMap<QString, Geom::Box> boxMap;

public slots:
	// entire mesh
	void setMesh(Model* model);

	// graph
	void createScaffold(int method);
	void saveScaffold();
	void loadScaffold();

	// node
	void refitSelectedNodes(int method);
	void changeTypeOfSelectedNodes();

	// link
	void linkSelectedNodes();

	// visualization
	void showCuboids(int state);
	void showScaffold(int state);
	void showMeshes(int state);
	void showAABB(int state);

signals:
	void sceneSettingsChanged();
	void scaffoldModified();
	void scaffoldChanged(QString name);
	void message(QString msg);
};


