#ifndef TNODELIST_H
#define TNODELIST_H

#include "primitives/transaction.h"
#include "platformstyle.h"
#include "sync.h"
#include "util.h"

#include <QMenu>
#include <QTimer>
#include <QWidget>

#define MY_TNODELIST_UPDATE_SECONDS                 60
#define TNODELIST_UPDATE_SECONDS                    15
#define TNODELIST_FILTER_COOLDOWN_SECONDS            3

namespace Ui {
    class TnodeList;
}

class ClientModel;
class WalletModel;

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Tnode Manager page widget */
class TnodeList : public QWidget
{
    Q_OBJECT

public:
    explicit TnodeList(const PlatformStyle *platformStyle, QWidget *parent = 0);
    ~TnodeList();

    void setClientModel(ClientModel *clientModel);
    void setWalletModel(WalletModel *walletModel);
    void StartAlias(std::string strAlias);
    void StartAll(std::string strCommand = "start-all");

private:
    QMenu *contextMenu;
    int64_t nTimeFilterUpdated;
    bool fFilterUpdated;

public Q_SLOTS:
    void updateMyTnodeInfo(QString strAlias, QString strAddr, const COutPoint& outpoint);
    void updateMyNodeList(bool fForce = false);
    void updateNodeList();

Q_SIGNALS:

private:
    QTimer *timer;
    Ui::TnodeList *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;

    // Protects tableWidgetTnodes
    CCriticalSection cs_mnlist;

    // Protects tableWidgetMyTnodes
    CCriticalSection cs_mymnlist;

    QString strCurrentFilter;

private Q_SLOTS:
    void showContextMenu(const QPoint &);
    void on_filterLineEdit_textChanged(const QString &strFilterIn);
    void on_startButton_clicked();
    void on_startAllButton_clicked();
    void on_startMissingButton_clicked();
    void on_tableWidgetMyTnodes_itemSelectionChanged();
    void on_UpdateButton_clicked();
};
#endif // TNODELIST_H
