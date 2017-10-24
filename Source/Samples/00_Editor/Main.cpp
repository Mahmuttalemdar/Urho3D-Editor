#include <Urho3D/AngelScript/ScriptFile.h>
#include <Urho3D/Audio/Sound.h>
#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Engine/Application.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Graphics/Animation.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/DebugRenderer.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/StaticModel.h>
#include <Urho3D/Graphics/Technique.h>
#include <Urho3D/Graphics/ParticleEffect.h>
#include <Urho3D/Graphics/ParticleEmitter.h>
#include <Urho3D/Graphics/Texture3D.h>
#include <Urho3D/Graphics/TextureCube.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/LuaScript/LuaFile.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/UI/Font.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/UI.h>
#include <Urho3D/Urho2D/AnimationSet2D.h>
#include <Urho3D/Urho2D/ParticleEffect2D.h>

#include "../../Library/AbstractUI/AbstractUI.h"
#include "../../Library/AbstractUI/KeyBinding.h"
#include "../../Library/AbstractUI/Urho/UrhoUI.h"
#include "../../Library/AbstractUI/Qt/QtUI.h"
#include "../../Library/Editor/CameraController.h"
#include "../../Library/Editor/Editor.h"
#include "../../Library/Editor/Selection.h"
#include "../../Library/Editor/HierarchyWindow.h"
#include "../../Library/Editor/ObjectSelector.h"
#include "../../Library/Editor/EditorViewportLayout.h"
#include "../../Library/Editor/DebugGeometryRenderer.h"
#include "../../Library/Editor/Gizmo.h"
#include "../../Library/Editor/ResourceBrowser.h"
#include "../../Library/Editor/Inspector.h"
#include "../../Library/Editor/Transformable.h"

#include <Urho3D/DebugNew.h>

using namespace Urho3D;

void CreateScene(Scene* scene)
{
    ResourceCache* cache = scene->GetSubsystem<ResourceCache>();

    // Create the Octree component to the scene. This is required before adding any drawable components, or else nothing will
    // show up. The default octree volume will be from (-1000, -1000, -1000) to (1000, 1000, 1000) in world coordinates; it
    // is also legal to place objects outside the volume but their visibility can then not be checked in a hierarchically
    // optimizing manner
    scene->CreateComponent<Octree>();
    scene->CreateComponent<DebugRenderer>();

    // Create a child scene node (at world origin) and a StaticModel component into it. Set the StaticModel to show a simple
    // plane mesh with a "stone" material. Note that naming the scene nodes is optional. Scale the scene node larger
    // (100 x 100 world units)
    Node* planeNode = scene->CreateChild("Plane");
    planeNode->SetScale(Vector3(100.0f, 1.0f, 100.0f));
    StaticModel* planeObject = planeNode->CreateComponent<StaticModel>();
    planeObject->SetModel(cache->GetResource<Model>("Models/Plane.mdl"));
    planeObject->SetMaterial(cache->GetResource<Material>("Materials/StoneTiled.xml"));

    // Create a directional light to the world so that we can see something. The light scene node's orientation controls the
    // light direction; we will use the SetDirection() function which calculates the orientation from a forward direction vector.
    // The light will use default settings (white light, no shadows)
    Node* lightNode = scene->CreateChild("DirectionalLight");
    lightNode->SetDirection(Vector3(0.6f, -1.0f, 0.8f)); // The direction vector does not need to be normalized
    Light* light = lightNode->CreateComponent<Light>();
    light->SetLightType(LIGHT_DIRECTIONAL);

    // Create more StaticModel objects to the scene, randomly positioned, rotated and scaled. For rotation, we construct a
    // quaternion from Euler angles where the Y angle (rotation about the Y axis) is randomized. The mushroom model contains
    // LOD levels, so the StaticModel component will automatically select the LOD level according to the view distance (you'll
    // see the model get simpler as it moves further away). Finally, rendering a large number of the same object with the
    // same material allows instancing to be used, if the GPU supports it. This reduces the amount of CPU work in rendering the
    // scene.
    const unsigned NUM_OBJECTS = 200;
    for (unsigned i = 0; i < NUM_OBJECTS; ++i)
    {
        Node* mushroomNode = scene->CreateChild("Mushroom");
        mushroomNode->SetPosition(Vector3(Random(90.0f) - 45.0f, 0.0f, Random(90.0f) - 45.0f));
        mushroomNode->SetRotation(Quaternion(0.0f, Random(360.0f), 0.0f));
        mushroomNode->SetScale(0.5f + Random(2.0f));
        StaticModel* mushroomObject = mushroomNode->CreateComponent<StaticModel>();
        mushroomObject->SetModel(cache->GetResource<Model>("Models/Mushroom.mdl"));
        mushroomObject->SetMaterial(cache->GetResource<Material>("Materials/Mushroom.xml"));
    }

    // Create a scene node for the camera, which we will move around
    // The camera will use default settings (1000 far clip distance, 45 degrees FOV, set aspect ratio automatically)
    Node* cameraNode = scene->CreateChild("Camera");
    cameraNode->CreateComponent<Camera>();

    // Set an initial position for the camera scene node above the plane
    cameraNode->SetPosition(Vector3(0.0f, 5.0f, 0.0f));
}

class StandardDocument : public Object
{
    URHO3D_OBJECT(StandardDocument, Object);

public:
    StandardDocument(Context* context) : Object(context) { }

    String resourceKey_;
    SharedPtr<Resource> resource_;
    SharedPtr<Scene> scene_;
    SharedPtr<Selection> selection_;
};

class DefaultEditor : public Object
{
    URHO3D_OBJECT(DefaultEditor, Object);

public:
    template <class T> StandardDocument* FindDocument(AbstractMainWindow* mainWindow, T condition)
    {
        for (Object* document : mainWindow->GetDocuments())
            if (condition(document))
                return static_cast<StandardDocument*>(document);
        return nullptr;
    }

    static SharedPtr<StandardDocument> CreateSceneDocument(Scene* scene, const String& resourceKey = String::EMPTY)
    {
        Context* context = scene->GetContext();
        auto document = MakeShared<StandardDocument>(context);
        document->resourceKey_ = resourceKey;
        document->scene_ = scene;
        document->selection_ = MakeShared<Selection>(context);
        return document;
    }

    DefaultEditor(AbstractMainWindow* mainWindow, bool blenderHotkeys)
        : Object(mainWindow->GetContext())
    {
        // Construct widgets and subsystems
        editor_ = MakeShared<Editor>(mainWindow);
        viewportLayout_ = MakeShared<EditorViewportLayout>(context_);
        debugGeometryRenderer_ = MakeShared<DebugGeometryRenderer>(context_);
        hierarchyWindow_ = MakeShared<HierarchyWindow1>(mainWindow);
        resourceBrowser_ = MakeShared<ResourceBrowser>(mainWindow);
        inspector_ = MakeShared<Inspector>(mainWindow);

        mainWindow->onCurrentDocumentChanged_ = [=](Object* object)
        {
            StandardDocument* document = static_cast<StandardDocument*>(object);
            if (document->scene_)
            {
                HierarchyWindow* hierarchy = hierarchyWindow_->GetDocument(document);
                hierarchy->SetScene(document->scene_);
                hierarchy->SetSelection(document->selection_);
                hierarchyWindow_->SelectDocument(document);

                viewportLayout_->SetScene(document->scene_);
            }
        };

        scene_ = MakeShared<Scene>(context_);
        mainWindow->InsertDocument(CreateSceneDocument(scene_), "New Scene", 0);
        CreateScene(scene_);


        viewportLayout_->SetScene(scene_);
        viewportLayout_->SetCameraTransform(scene_->GetChild("Camera"));

        auto editorContext = MakeShared<StandardEditorContext>(context_, viewportLayout_);

        auto cameraController = MakeShared<CameraController>(context_);
        cameraController->SetCamera(&viewportLayout_->GetCurrentCamera());
        cameraController->SetSpeed(Vector3::ONE * 5.0f);
        cameraController->SetPanSpeed(Vector2::ONE * 2.5f);
        cameraController->SetAccelerationFactor(Vector3::ONE * 5.0f);
        cameraController->SetRotationSpeed(Vector2::ONE * 0.2f);

        auto selection = MakeShared<Selection>(context_);
        auto objectSelector = MakeShared<ObjectSelector>(context_);
        objectSelector->SetScene(scene_);
        objectSelector->SetSelection(selection);
        objectSelector->AddSelectionTransferring("TerrainPatch", "Terrain");

        auto selectionTransform = MakeShared<SelectionTransform>(context_);
        selectionTransform->SetSelection(selection);
        selectionTransform->SetScene(scene_);

        auto gizmo = MakeShared<Gizmo>(context_);
        gizmo->SetGizmoType(GizmoType::Position);
        gizmo->SetTransformable(selectionTransform);

        debugGeometryRenderer_->SetScene(scene_);
        debugGeometryRenderer_->SetSelection(selection);
        debugGeometryRenderer_->DisableForComponent("Terrain");

        editor_->SetEditorContext(editorContext);
        editor_->AddOverlay(viewportLayout_);
        editor_->AddOverlay(gizmo);
        editor_->AddOverlay(cameraController);
        editor_->AddOverlay(objectSelector);
        editor_->AddOverlay(debugGeometryRenderer_);
        editor_->AddSubsystem(selectionTransform);

//         {
//             AbstractDock* dialog = mainWindow->AddDock(DockLocation::Left);
//             dialog->SetName("View3D");
//
//             AbstractView3D* view = dialog->CreateContent<AbstractView3D>();
//             view->SetAutoUpdate(false);
//             view->SetView(scene_, scene_->GetChild("Camera")->GetComponent<Camera>());
//         }

        resourceBrowser_->AddXmlExtension(".xml");
        resourceBrowser_->AddLayers(MakeExtensionLayers<Font>({ ".ttf", ".otf" }));
        resourceBrowser_->AddLayers(MakeExtensionLayers<Sound>({ ".ogg", ".wav" }));
        resourceBrowser_->AddLayers(MakeExtensionLayers<Image>({ ".dds", ".png", ".jpg", ".jpeg", ".hdr", ".bmp", ".tga", ".ktx", ".pvr" }));
        resourceBrowser_->AddLayers(MakeExtensionLayers({ ".obj", ".fbx", ".dae", ".blend" }, "Raw Model"));
        resourceBrowser_->AddLayer(MakeExtensionLayer<ScriptFile>(".as"));
        resourceBrowser_->AddLayer(MakeExtensionLayer<LuaFile>(".lua"));
        resourceBrowser_->AddLayers(MakeExtensionLayers({ ".hlsl", ".glsl" }, "Shader"));
        resourceBrowser_->AddLayer(MakeExtensionLayer(".html", "HTML"));
        resourceBrowser_->AddLayer(MakeBinaryLayer<Scene>("USCN"));
        resourceBrowser_->AddLayer(MakeBinaryLayer("UPAK", "Package"));
        resourceBrowser_->AddLayer(MakeBinaryLayer("ULZ4", "Compressed Package"));
        resourceBrowser_->AddLayer(MakeBinaryLayer<ScriptFile>("ASBC"));
        resourceBrowser_->AddLayers(MakeBinaryLayers<Model>({ "UMDL", "UMD2" }));
        resourceBrowser_->AddLayer(MakeBinaryLayer("USHD", "Compiled Shader"));
        resourceBrowser_->AddLayer(MakeBinaryLayer<Animation>("UANI"));
        resourceBrowser_->AddLayer(MakeXmlLayer<Scene>("scene"));
        resourceBrowser_->AddLayer(MakeXmlLayer<Node>("node"));
        resourceBrowser_->AddLayer(MakeXmlLayer<Material>("material"));
        resourceBrowser_->AddLayer(MakeXmlLayer<Technique>("technique"));
        resourceBrowser_->AddLayer(MakeXmlLayer<ParticleEffect>("particleeffect"));
        resourceBrowser_->AddLayer(MakeXmlLayer<ParticleEmitter>("particleemitter"));
        resourceBrowser_->AddLayer(MakeXmlLayer<Texture2D>("texture"));
        resourceBrowser_->AddLayer(MakeXmlLayer("element", "UI Element"));
        resourceBrowser_->AddLayer(MakeXmlLayer("elements", "UI Elements"));
        resourceBrowser_->AddLayer(MakeXmlLayer("animation", "Animation Metadata"));
        resourceBrowser_->AddLayer(MakeXmlLayer("renderpath", "Render Path"));
        resourceBrowser_->AddLayer(MakeXmlLayer("TextureAtlas", "Texture Atlas"));
        resourceBrowser_->AddLayer(MakeXmlLayer<ParticleEffect2D>("particleEmitterConfig"));
        resourceBrowser_->AddLayer(MakeXmlLayer<Texture3D>("texture3d"));
        resourceBrowser_->AddLayer(MakeXmlLayer<TextureCube>("cubemap"));
        resourceBrowser_->AddLayer(MakeXmlLayer<AnimationSet2D>("spriter_data"));
        resourceBrowser_->AddLayer(MakeExtensionLayer<XMLFile>(".xml"));
        resourceBrowser_->ScanResources();

        resourceBrowser_->onResourceDoubleClicked_ = [=](const ResourceFileDesc& file)
        {
            if (file.type_.objectType_ == Scene::GetTypeStatic())
            {
                StandardDocument* existingDocument = FindDocument(mainWindow,
                    [&file](Object* object)
                {
                    StandardDocument* document = static_cast<StandardDocument*>(object);
                    return document->resourceKey_ == file.resourceKey_;
                });

                if (!existingDocument)
                {
                    ResourceCache* cache = GetSubsystem<ResourceCache>();
                    SharedPtr<XMLFile> xml = cache->GetTempResource<XMLFile>(file.resourceKey_);
                    auto scene = MakeShared<Scene>(context_);
                    scene->LoadXML(xml->GetRoot());

                    SharedPtr<StandardDocument> document = CreateSceneDocument(scene, file.resourceKey_);
                    mainWindow->InsertDocument(document, file.name_, 0);

                    existingDocument = document;
                }

                mainWindow->SelectDocument(existingDocument);
            }
        };

        auto inspectable = MakeShared<MultiplePanelInspectable>(context_);
        for (int i = 0; i < 10; ++i)
        {
            auto attributeMetadataInjector = MakeShared<AttributeMetadataInjector>(context_);
            attributeMetadataInjector->AddMetadata(Node::GetTypeStatic(), "Position", AttributeMetadata::P_APPLY_ON_COMMIT, true);

            auto inspectorPanel = MakeShared<MultipleSerializableInspectorPanel>(context_);
            inspectorPanel->SetMetadataInjector(attributeMetadataInjector);
            inspectorPanel->SetMaxLabelLength(100);
            inspectorPanel->AddObject(scene_->GetChildren()[10]);
            inspectorPanel->AddObject(scene_->GetChildren()[20]);

            inspectable->AddPanel(inspectorPanel);
        }
        inspector_->SetInspectable(inspectable);

        if (blenderHotkeys)
        {
            using KB = KeyBinding;
            using CC = CameraController;
            cameraController->SetFlyMode(false);
            cameraController->SetPositionControl(false);
            cameraController->SetControls({
                { CC::MOVE_FORWARD,     { KB::OPTIONAL_SHIFT + KB::Key(KEY_W), KB::OPTIONAL_SHIFT + KB::Key(KEY_UP)       } },
                { CC::MOVE_BACK,        { KB::OPTIONAL_SHIFT + KB::Key(KEY_S), KB::OPTIONAL_SHIFT + KB::Key(KEY_DOWN)     } },
                { CC::MOVE_LEFT,        { KB::OPTIONAL_SHIFT + KB::Key(KEY_A), KB::OPTIONAL_SHIFT + KB::Key(KEY_LEFT)     } },
                { CC::MOVE_RIGHT,       { KB::OPTIONAL_SHIFT + KB::Key(KEY_D), KB::OPTIONAL_SHIFT + KB::Key(KEY_RIGHT)    } },
                { CC::MOVE_UP,          { KB::OPTIONAL_SHIFT + KB::Key(KEY_E), KB::OPTIONAL_SHIFT + KB::Key(KEY_PAGEUP)   } },
                { CC::MOVE_DOWN,        { KB::OPTIONAL_SHIFT + KB::Key(KEY_Q), KB::OPTIONAL_SHIFT + KB::Key(KEY_PAGEDOWN) } },
                { CC::MOVE_ACCEL,       { KB::SHIFT } },
                { CC::TOGGLE_FLY_MODE,  { KB::SHIFT + KB::Key(KEY_F) } },
                { CC::RESET_FLY_MODE,   { KB::ANY_MODIFIER + KB::Key(KEY_ESCAPE), KB::ANY_MODIFIER + KB::Mouse(MOUSEB_RIGHT) } },
                { CC::ROTATE,           { } },
                { CC::ORBIT,            { KB::Mouse(MOUSEB_MIDDLE) } },
                { CC::PAN,              { KB::SHIFT + KB::Mouse(MOUSEB_MIDDLE) } },
                { CC::WHEEL_SCROLL_X,   { KB::CTRL } },
                { CC::WHEEL_SCROLL_Y,   { KB::SHIFT } },
                { CC::WHEEL_SCROLL_Z,   { KB::ANY_MODIFIER } },
                { CC::WHEEL_ZOOM,       { KB::ALT } },
            });

            using OS = ObjectSelector;
            objectSelector->SetControls({
                { OS::SELECT_NODE,      { KB::Mouse(MOUSEB_LEFT)                        } },
                { OS::TOGGLE_NODE,      { KB::Mouse(MOUSEB_LEFT) + KB::SHIFT            } },
                { OS::SELECT_COMPONENT, { KB::Mouse(MOUSEB_LEFT) + KB::CTRL             } },
                { OS::TOGGLE_COMPONENT, { KB::Mouse(MOUSEB_LEFT) + KB::SHIFT + KB::CTRL } },
            });
        }
        else
        {
            using KB = KeyBinding;
            using CC = CameraController;
            cameraController->SetFlyMode(false);
            cameraController->SetPositionControl(true);
            cameraController->SetControls({
                { CC::MOVE_FORWARD,     { KB::OPTIONAL_SHIFT + KB::Key(KEY_W), KB::OPTIONAL_SHIFT + KB::Key(KEY_UP)       } },
                { CC::MOVE_BACK,        { KB::OPTIONAL_SHIFT + KB::Key(KEY_S), KB::OPTIONAL_SHIFT + KB::Key(KEY_DOWN)     } },
                { CC::MOVE_LEFT,        { KB::OPTIONAL_SHIFT + KB::Key(KEY_A), KB::OPTIONAL_SHIFT + KB::Key(KEY_LEFT)     } },
                { CC::MOVE_RIGHT,       { KB::OPTIONAL_SHIFT + KB::Key(KEY_D), KB::OPTIONAL_SHIFT + KB::Key(KEY_RIGHT)    } },
                { CC::MOVE_UP,          { KB::OPTIONAL_SHIFT + KB::Key(KEY_E), KB::OPTIONAL_SHIFT + KB::Key(KEY_PAGEUP)   } },
                { CC::MOVE_DOWN,        { KB::OPTIONAL_SHIFT + KB::Key(KEY_Q), KB::OPTIONAL_SHIFT + KB::Key(KEY_PAGEDOWN) } },
                { CC::MOVE_ACCEL,       { KB::SHIFT } },
                { CC::ROTATE,           { KB::OPTIONAL_SHIFT + KB::Mouse(MOUSEB_RIGHT) } },
                { CC::ORBIT,            { KB::Mouse(MOUSEB_MIDDLE) } },
                { CC::PAN,              { KB::SHIFT + KB::Mouse(MOUSEB_MIDDLE) } },
                { CC::WHEEL_SCROLL_Z,   { KB::ANY_MODIFIER } },
                { CC::WHEEL_ZOOM,       { KB::ALT } },
            });

            using OS = ObjectSelector;
            objectSelector->SetControls({
                { OS::SELECT_NODE,      { KB::Mouse(MOUSEB_LEFT)                        } },
                { OS::TOGGLE_NODE,      { KB::Mouse(MOUSEB_LEFT) + KB::CTRL             } },
                { OS::SELECT_COMPONENT, { KB::Mouse(MOUSEB_LEFT) + KB::SHIFT            } },
                { OS::TOGGLE_COMPONENT, { KB::Mouse(MOUSEB_LEFT) + KB::CTRL + KB::SHIFT } },
            });
        }

        mainWindow->AddAction("EditCut", KeyBinding::Key(KEY_X) + KeyBinding::CTRL, [=]() {});
        mainWindow->AddAction("EditCopy", KeyBinding::Key(KEY_C) + KeyBinding::CTRL, [=]() {});
        mainWindow->AddAction("EditPaste", KeyBinding::Key(KEY_V) + KeyBinding::CTRL, [=]() {});
        mainWindow->AddAction("EditDelete", KeyBinding::Key(KEY_DELETE),
            [=]()
        {
            // #TODO Implement me
            selection->GetSelectedNodesAndComponents();
        });

        AbstractMenu* menuEdit = mainWindow->AddMenu("Edit");
        menuEdit->AddAction("Cut", "EditCut");
        menuEdit->AddAction("Copy", "EditCopy");
        menuEdit->AddAction("Paste", "EditPaste");
        menuEdit->AddAction("Delete", "EditDelete");
    }

private:
    SharedPtr<Editor> editor_;
    SharedPtr<EditorViewportLayout> viewportLayout_;
    SharedPtr<DebugGeometryRenderer> debugGeometryRenderer_;

    SharedPtr<HierarchyWindow1> hierarchyWindow_;
    SharedPtr<Inspector> inspector_;
    SharedPtr<ResourceBrowser> resourceBrowser_;
    SharedPtr<Scene> scene_;
};

int QtEditorMain()
{
    static int argcStub = 0;
    static char* argvStub[] = { nullptr };
    QApplication::setStyle("Fusion");
    QApplication applicaton(argcStub, argvStub);
    QtMainWindow mainWindow(applicaton);
    DefaultEditor defaultEditor(&mainWindow, false);
    mainWindow.showMaximized();
    return applicaton.exec();
}

//////////////////////////////////////////////////////////////////////////
class UrhoEditorApplication : public Application
{
    URHO3D_OBJECT(UrhoEditorApplication, Application);

public:
    UrhoEditorApplication(Context* context) : Application(context) { }

    virtual void Start() override
    {
        Application::Start();

        Input* input = GetSubsystem<Input>();
        UI* ui = GetSubsystem<UI>();
        ResourceCache* cache = GetSubsystem<ResourceCache>();
        XMLFile* style = cache->GetResource<XMLFile>("UI/DefaultStyle.xml");
        input->SetMouseVisible(true);
        ui->GetRoot()->SetDefaultStyle(style);

        mainWindow_ = MakeShared<UrhoMainWindow>(context_);
        editor_ = MakeShared<DefaultEditor>(mainWindow_, false);
    }

private:
    SharedPtr<UrhoMainWindow> mainWindow_;
    SharedPtr<DefaultEditor> editor_;
};


URHO3D_DEFINE_MAIN(QtEditorMain())
// URHO3D_DEFINE_APPLICATION_MAIN(UrhoEditorApplication)