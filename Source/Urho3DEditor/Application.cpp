#include "Application.h"
#include <Urho3D/Core/ProcessUtils.h>
#include <Urho3D/Engine/EngineDefs.h>
#include <QFile>
#include <QHBoxLayout>
#include <QTabBar>
#include <QTimer>
#include <QResource>

#include "Configuration.h"
#include "Core/Core.h"
#include "Urho3DProject.h"
#include "SceneEditor/AttributeInspector.h"
#include "SceneEditor/HierarchyWindow.h"
#include "SceneEditor/SceneEditor.h"
#include "SceneEditor/SceneDocument.h"

namespace Urho3DEditor
{

Application::Application(int argc, char** argv)
    : QApplication(argc, argv)
{
    Q_INIT_RESOURCE(Editor);
    Q_INIT_RESOURCE(QDarkStyle);
}

Application::~Application()
{
}

int Application::Run()
{
    if (!Initialize())
        return 1;
    mainWindowWidget_->showMaximized();
    return exec();
}

bool Application::Initialize()
{
    // Setup style
    QFile file(":/qdarkstyle/style.qss");
    if (file.open(QFile::ReadOnly | QFile::Text))
        setStyleSheet(QLatin1String(file.readAll()));

    mainWindowWidget_.reset(new QMainWindow());
    config_.reset(new Configuration());
    core_.reset(new Core(*config_, *mainWindowWidget_));
    moduleSystem_.reset(new ModuleSystem(*config_, *core_));
    if (!core_->Initialize())
        return false;

    core_->RegisterFilter(tr("Urho3D Scenes and Projects (*.xml *.json *.bin *.urho)"),
    { SceneDocument::GetStaticName(), ProjectDocument::GetStaticName() });
    core_->RegisterDocument(SceneDocument::GetStaticDescription());
    core_->RegisterDocument(ProjectDocument::GetStaticDescription());
    core_->RegisterFilter(tr("Any files (*.*)"), {});

    moduleSystem_->AddModule(new SceneEditor());
    moduleSystem_->AddModule(new HierarchyWindow());
    moduleSystem_->AddModule(new AttributeInspector());

    core_->LoadLayout();
    return true;
}

// void SceneEditorPlugin::Register(EditorInterface& editor)
// {
//     QMenu* menuFile = editor.GetMainMenu("File");
//     QAction* menuNewScene = menuFile->addAction("New Scene");
//     connect(menuNewScene, SIGNAL(triggered()), this, SLOT(HandleNewScene()));
// }
//
// void SceneEditorPlugin::Unregister(EditorInterface& editor)
// {
//
// }
//
// void SceneEditorPlugin::HandleNewScene()
// {
//
// }

}
