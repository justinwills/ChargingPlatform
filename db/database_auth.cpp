#include "database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>
#include <QStringList>

// ---------- 管理员登录 / 用户登录 / 登录与操作日志 ----------

bool Database::checkAdminLogin(const QString &username, const QString &password)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("select id from admins where username = ? and password = ?");
    query.addBindValue(username);
    query.addBindValue(password);
    if (!query.exec()) {
        qDebug() << "checkAdminLogin 查询失败：" << query.lastError().text();
        return false;
    }
    return query.next(); // 查到一行说明账号密码匹配
}

// ================= 用户 =================
bool Database::phoneLogin(const QString &phone, UserInfo *outUser)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("select id, phone, nickname, avatar_path, balance, status, created_at "
                   "from users where phone = ?");
    query.addBindValue(phone);
    if (!query.exec()) {
        qDebug() << "phoneLogin 查询失败：" << query.lastError().text();
        return false;
    }

    if (query.next()) {
        // 手机号已存在，直接登录
        if (outUser) {
            outUser->id = query.value(0).toInt();
            outUser->phone = query.value(1).toString();
            outUser->nickname = query.value(2).toString();
            outUser->avatarPath = query.value(3).toString();
            outUser->balance = query.value(4).toDouble();
            outUser->status = query.value(5).toString();
            outUser->createdAt = query.value(6).toString();
        }
        logLoginRecord(phone);
        return true;
    }

    // 不存在则自动注册，默认昵称"用户+手机号后4位"
    QString defaultNickname = "用户" + phone.right(4);
    QSqlQuery insertQuery(currentThreadDb());
    insertQuery.prepare("insert into users(phone, nickname, balance, status) "
                         "values(?, ?, 0.0, '正常')");
    insertQuery.addBindValue(phone);
    insertQuery.addBindValue(defaultNickname);
    if (!insertQuery.exec()) {
        qDebug() << "phoneLogin 自动注册失败：" << insertQuery.lastError().text();
        return false;
    }

    if (outUser) {
        outUser->id = insertQuery.lastInsertId().toInt();
        outUser->phone = phone;
        outUser->nickname = defaultNickname;
        outUser->avatarPath = QString();
        outUser->balance = 0;
        outUser->status = "正常";
        outUser->createdAt = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    }
    logLoginRecord(phone);
    return true;
}

// ---------- 登录记录（第31项）----------
bool Database::logLoginRecord(const QString &phone, const QString &ipAddress)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("insert into login_logs(phone, ip_address) values(?, ?)");
    query.addBindValue(phone);
    query.addBindValue(ipAddress);
    if (!query.exec()) {
        qDebug() << "logLoginRecord 写入失败：" << query.lastError().text();
        return false;
    }
    return true;
}

QList<LoginLogInfo> Database::getLoginHistory(const QString &phone, int limit)
{
    QList<LoginLogInfo> list;
    QSqlQuery query(currentThreadDb());
    if (phone.isEmpty()) {
        query.prepare("select id, phone, login_time, ip_address from login_logs "
                       "order by login_time desc limit ?");
        query.addBindValue(limit);
    } else {
        query.prepare("select id, phone, login_time, ip_address from login_logs "
                       "where phone = ? order by login_time desc limit ?");
        query.addBindValue(phone);
        query.addBindValue(limit);
    }
    if (!query.exec()) {
        qDebug() << "getLoginHistory 查询失败：" << query.lastError().text();
        return list;
    }
    while (query.next()) {
        LoginLogInfo info;
        info.id = query.value(0).toInt();
        info.phone = query.value(1).toString();
        info.loginTime = query.value(2).toString();
        info.ipAddress = query.value(3).toString();
        list.append(info);
    }
    return list;
}

bool Database::logOperation(int operatorId, const QString &operatorType,
                            const QString &operationType, const QString &targetTable,
                            int targetId, const QString &content)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("insert into operation_logs(operator_id, operator_type, operation_type, "
                  "target_table, target_id, content) values(?, ?, ?, ?, ?, ?)");
    query.addBindValue(operatorId);
    query.addBindValue(operatorType);
    query.addBindValue(operationType);
    query.addBindValue(targetTable);
    query.addBindValue(targetId);
    query.addBindValue(content);
    if (!query.exec()) {
        qDebug() << "logOperation 写入失败：" << query.lastError().text();
        return false;
    }
    return true;
}

QList<OperationLogInfo> Database::getOperationLogs(const QString &targetTable,
                                                    int operatorId,
                                                    int limit)
{
    QList<OperationLogInfo> list;
    QSqlQuery query(currentThreadDb());

    QString sql = "select id, operator_id, operator_type, operation_type, target_table, "
                  "target_id, content, operation_time from operation_logs";
    QStringList conditions;
    if (!targetTable.isEmpty()) {
        conditions << "target_table = ?";
    }
    if (operatorId >= 0) {
        conditions << "operator_id = ?";
    }
    if (!conditions.isEmpty()) {
        sql += " where " + conditions.join(" and ");
    }
    sql += " order by operation_time desc limit ?";

    query.prepare(sql);
    if (!targetTable.isEmpty()) {
        query.addBindValue(targetTable);
    }
    if (operatorId >= 0) {
        query.addBindValue(operatorId);
    }
    query.addBindValue(limit);

    if (!query.exec()) {
        qDebug() << "getOperationLogs 查询失败：" << query.lastError().text();
        return list;
    }

    while (query.next()) {
        OperationLogInfo info;
        info.id = query.value(0).toInt();
        info.operatorId = query.value(1).toInt();
        info.operatorType = query.value(2).toString();
        info.operationType = query.value(3).toString();
        info.targetTable = query.value(4).toString();
        info.targetId = query.value(5).toInt();
        info.content = query.value(6).toString();
        info.operationTime = query.value(7).toString();
        list.append(info);
    }
    return list;
}

