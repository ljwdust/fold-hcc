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
	void createScaffold(bool doFitting = true);
	void saveScaffold();
	void loadScaffold();

	// node
	void refitSelectedNodes(int method);
	void changeTypeOfSelectedNodes();

	// visualization
	void showCuboids(int state);
	void showScaffold(int state);

signals:
	void sceneSettingsChanged();
	void scaffoldChanged();
	void message(QString msg);
};


