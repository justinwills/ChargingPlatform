#include "requestdispatcher.h"
#include "database.h"

// ---------- 用户端：登录/自动注册、资料更新、余额充值 ----------

QJsonObject RequestDispatcher::handleLogin(const QJsonObject &params)
{
    QString phone = params.value("phone").toString().trimmed();

    // 虽然客户端已验证但是为了避免错误，服务器端在验证多一次
    static const QRegularExpression phoneRegex("^1\\d{10}$");
    if (!phoneRegex.match(phone).hasMatch()) {
        return fail(1, "手机号格式错误，应为11位手机号");
    }

    UserInfo user;
    if (!Database::phoneLogin(phone, &user)) {
        return fail(3, "数据库操作失败");
    }

    if(user.status == "冻结"){
        return fail(2,"该账号已被冻结，无法登录");
    }

    QJsonObject data;
    data["userId"] = user.id;
    data["phone"] = user.phone;
    data["nickname"] = user.nickname;
    data["avatarPath"] = user.avatarPath;
    data["balance"] = user.balance;
    data["status"] = user.status;
    data["createdAt"] = user.createdAt;

    int ongoingOrderId = -1;
    if (Database::hasOngoingOrder(user.id, &ongoingOrderId)) {
        data["ongoingOrderId"] = ongoingOrderId;
    }
    return ok(data);
}

QJsonObject RequestDispatcher::handleUpdateUserProfile(const QJsonObject &params)
{
    if (!params.contains("userId")){
        return fail(1,"缺少userId参数");
    }

    int userId = params.value("userId").toInt();
    QString nickname = params.value("nickname").toString().trimmed();
    QString avatarPath = params.value("avatarPath").toString();

    if (userId <= 0 || nickname.isEmpty()) {
        return fail(1, "用户id或昵称无效");
    }
    if (!Database::updateUserProfile(userId, nickname, avatarPath)) {
        return fail(2, "更新用户信息失败");
    }

    // 更新后重新查询，向客户端返回数据库中的最新记录
    UserInfo user;
    if (!Database::getUserById(userId, &user)) {
        return fail(3, "读取更新后的用户信息失败");
    }

    QJsonObject data;
    data["userId"] = user.id;
    data["phone"] = user.phone;
    data["nickname"] = user.nickname;
    data["avatarPath"] = user.avatarPath;
    data["balance"] = user.balance;
    data["status"] = user.status;
    data["createdAt"] = user.createdAt;

    return ok(data);
}

QJsonObject RequestDispatcher::handleRechargeBalance(const QJsonObject &params)
{
    if(!params.contains("userId") || !params.contains("amount")){
        return fail(1,"缺少userId或amount参数");
    }

    int userId = params.value("userId").toInt();
    double amount = params.value("amount").toDouble();

    if(userId <= 0){
        return fail(1,"用户ID不存在");
    }

    if(amount <= 0){
        return fail(1,"充值金额必须大于0");
    }

    if (!Database::rechargeBalance(userId, amount)) {
        return fail(2, "充值失败");
    }

    UserInfo user;
    if (!Database::getUserById(userId, &user)) {
        return fail(3, "读取充值后的用户信息失败");
    }

    QJsonObject data;
    data["userId"] = user.id;
    data["phone"] = user.phone;
    data["nickname"] = user.nickname;
    data["avatarPath"] = user.avatarPath;
    data["balance"] = user.balance;
    data["status"] = user.status;
    data["createdAt"] = user.createdAt;

    return ok(data);
}

