#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>
#include <QDebug>
#include "serverlistener.h"
#include "clientconnection.h"
#include "database.h"

static int g_step = 0;
static int g_failures = 0;
static ClientConnection *g_client = nullptr;

struct StepDef {
    QString action;
    QJsonObject params;
    QString label;
};

static QVector<StepDef> g_steps = {
    {"login", {{"phone", "13800000001"}}, "老用户登录"},
    {"admin_login", {{"username", "admin"}, {"password", "123456"}}, "管理员登录-正确密码"},
    {"admin_login", {{"username", "admin"}, {"password", "wrong"}}, "管理员登录-错误密码"},
    {"query_stations", {}, "查询充电站列表"},
    {"query_pile_detail", {{"pileId", 1}}, "查询电桩详情(A01)"},
    {"start_charging", {{"userId", 1}, {"pileId", 1}}, "发起充电(A01,应成功)"},
    {"start_charging", {{"userId", 1}, {"pileId", 4}}, "重复发起充电(应被拒绝-已有进行中订单)"},
    {"query_order", {{"orderId", 1}}, "查询订单(应为充电中)"},
    {"settle_order", {{"orderId", 1}, {"amount", 10.5}, {"fee", 15.75}}, "结算订单"},
    {"query_order", {{"orderId", 1}}, "查询订单(应为已结算)"},
};

void sendCurrentStep()
{
    if (g_step >= g_steps.size()) {
        qDebug() << "\n===== 全部" << g_steps.size() << "个请求测试完毕，失败数:" << g_failures << "=====";
        QCoreApplication::exit(g_failures > 0 ? 1 : 0);
        return;
    }
    const auto &s = g_steps[g_step];
    qDebug().noquote() << QString("\n>> [%1/%2] 发送请求 action=%3  (%4)")
        .arg(g_step + 1).arg(g_steps.size()).arg(s.action, s.label);
    g_client->sendRequest(s.action, s.params);
}

void checkResponse(const QJsonObject &resp)
{
    const auto &s = g_steps[g_step];
    int code = resp.value("code").toInt(-999);
    QJsonObject data = resp.value("data").toObject();
    qDebug().noquote() << "   响应:" << QJsonDocument(resp).toJson(QJsonDocument::Compact);

    bool pass = true;
    QString note;

    if (s.action == "login" && g_step == 0) {
        pass = (code == 0 && data.value("nickname").toString() == "用户0001" && data.value("balance").toDouble() == 100.0);
        note = "期望 code=0, nickname=用户0001, balance=100";
    } else if (s.label.contains("正确密码")) {
        pass = (code == 0);
        note = "期望 code=0";
    } else if (s.label.contains("错误密码")) {
        pass = (code == 2);
        note = "期望 code=2（拒绝）";
    } else if (s.action == "query_stations") {
        QJsonArray arr = data.value("stations").toArray();
        pass = (code == 0 && arr.size() == 2 && arr[0].toObject().value("freePileCount").toInt() == 1);
        note = "期望 2个站点，站点1空闲电桩数=1";
    } else if (s.action == "query_pile_detail") {
        pass = (code == 0 && data.value("code").toString() == "A01");
        note = "期望电桩编号=A01";
    } else if (s.label.contains("应成功")) {
        pass = (code == 0 && data.value("orderId").toInt() == 1);
        note = "期望 code=0, orderId=1";
    } else if (s.label.contains("应被拒绝")) {
        pass = (code == 2);
        note = "期望 code=2（已有进行中订单，拒绝重复发起）";
    } else if (s.label.contains("应为充电中")) {
        pass = (code == 0 && data.value("status").toString() == "充电中");
        note = "期望 status=充电中";
    } else if (s.action == "settle_order") {
        pass = (code == 0);
        note = "期望 code=0";
    } else if (s.label.contains("应为已结算")) {
        pass = (code == 0 && data.value("status").toString() == "已结算" && data.value("fee").toDouble() == 15.75);
        note = "期望 status=已结算, fee=15.75";
    }

    qDebug().noquote() << QString("   结果: %1  (%2)").arg(pass ? "✓ 通过" : "✗ 失败").arg(note);
    if (!pass) g_failures++;

    g_step++;
    sendCurrentStep();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if (!Database::init("protocol_test.db")) {
        qDebug() << "数据库初始化失败";
        return 1;
    }

    auto *server = new ServerListener(&app);
    QObject::connect(server, &ServerListener::clientLog, &app, [](const QString &msg) {
        qDebug().noquote() << "[server]" << msg;
    });
    if (!server->listen(QHostAddress::LocalHost, 17799)) {
        qDebug() << "监听失败:" << server->errorString();
        return 1;
    }

    g_client = new ClientConnection(&app);
    QObject::connect(g_client, &ClientConnection::connected, &app, []() {
        qDebug() << "===== 客户端已连接服务端，开始逐条测试7种action =====";
        sendCurrentStep();
    });
    QObject::connect(g_client, &ClientConnection::responseReceived, &app, checkResponse);
    QObject::connect(g_client, &ClientConnection::connectionError, &app, [](const QString &msg) {
        qDebug() << "[client] 错误:" << msg;
    });

    g_client->connectToServer("127.0.0.1", 17799);

    QTimer::singleShot(10000, &app, []() {
        qDebug() << "===== 超时保护触发 =====";
        QCoreApplication::exit(1);
    });

    return app.exec();
}
