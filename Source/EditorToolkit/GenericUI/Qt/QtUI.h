#pragma once

#include "../GenericUI.h"
#include "../UrhoUI.h"
#include "QtUrhoWidget.h"
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QTreeView>
#include <QAbstractItemModel>
#include <QHeaderView>
#include <QAction>

namespace Urho3D
{

class QtMainWindow;

class QtDockDialog : public GenericDialog, public QDockWidget
{

public:
    QtDockDialog(AbstractMainWindow& mainWindow, GenericWidget* parent) : GenericDialog(mainWindow, parent), QDockWidget() { }
    void SetBodyWidget(GenericWidget* widget) override;
    void SetName(const String& name) override;

};

class QtHierarchyListModel : public QAbstractItemModel
{
public:
    QtHierarchyListModel(AbstractMainWindow& mainWindow, GenericWidget* parent);

    void InsertItem(GenericHierarchyListItem* item, const QModelIndex& parentIndex);
    void RemoveItem(GenericHierarchyListItem* item, const QModelIndex& parentIndex, int hintRow = -1);

    QModelIndex GetIndex(GenericHierarchyListItem* item, QModelIndex hint = QModelIndex());
    GenericHierarchyListItem* GetItem(const QModelIndex& index) const;
private:

    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override { return 1; }

    GenericHierarchyListItem rootItem_;

};

class QtHierarchyList : public GenericHierarchyList, public QTreeView
{
public:
    QtHierarchyList(AbstractMainWindow& mainWindow, GenericWidget* parent);

    void AddItem(GenericHierarchyListItem* item, unsigned index, GenericHierarchyListItem* parent) override;
    void SelectItem(GenericHierarchyListItem* item) override;
    void DeselectItem(GenericHierarchyListItem* item) override;
    void GetSelection(ItemVector& result) override;

private:

    QtHierarchyListModel model_;

};

class QtMenu : public GenericMenu
{
public:
    QtMenu(QtMainWindow* host, QMenu* menu);
    QtMenu(QtMainWindow* host, QAction* action);
    GenericMenu* AddMenu(const String& name) override;
    GenericMenu* AddAction(const String& name, const String& actionId) override;
private:
    QtMainWindow* host_ = nullptr;
    QMenu* menu_ = nullptr;
    QAction* action_ = nullptr;
    QList<QtMenu> children_;
};

class QtMainWindow : public QMainWindow, public AbstractMainWindow
{
    Q_OBJECT

public:
    QtMainWindow(QApplication& application);
    ~QtMainWindow() override;

    GenericDialog* AddDialog(DialogLocationHint hint) override;
    void AddAction(const AbstractAction& actionDesc) override;
    GenericMenu* AddMenu(const String& name) override;

    Context* GetContext() override;
    AbstractInput* GetInput() override;

    QtUrhoWidget& GetUrhoWidget() { return urhoWidget_; }
    QAction* FindAction(const String& id) const;

private:
    URHO3D_IMPLEMENT_WIDGET_FACTORY(CreateHierarchyList, QtHierarchyList);

private:
    SharedPtr<Context> context_;
    QApplication& application_;
    QtUrhoWidget urhoWidget_;

    HashMap<String, QAction*> actions_;
    QList<QtMenu> menus_;
};

}