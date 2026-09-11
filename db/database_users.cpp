#include "database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

// ---------- 用户信息维护与用户管理 ----------

bool Database::getUserById(int userId, UserInfo *outUser)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("select id, phone, nickname, avatar_path, balance, status, created_at "
                   "from users where id = ?");
    query.addBindValue(userId);
    if (!query.exec() || !query.next()) {
        return false;
    }
    if (outUser) {
        outUser->id = query.value(0).toInt();
        outUser->phone = query.value(1).toString();
        outUser->nickname = query.value(2).toString();
        outUser->avatarPath = query.value(3).toString();
        outUser->balance = query.value(4).toDouble();
        outUser->status = query.value(5).toString();
        outUser->createdAt = query.value(6).toString();
    }
    return true;
}

bool Database::updateUserProfile(int userId, const QString &nickname, const QString &avatarPath)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("update users set nickname = ?, avatar_path = ? where id = ?");
    query.addBindValue(nickname);
    query.addBindValue(avatarPath);
    query.addBindValue(userId);
    if (!query.exec()) {
        qDebug() << "updateUserProfile 失败：" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool Database::rechargeBalance(int userId, double amount)
{
    if (amount <= 0) {
        qDebug() << "rechargeBalance 失败：充值金额必须为正数";
        return false;
    }
    QSqlQuery query(currentThreadDb());
    query.prepare("update users set balance = balance + ? where id = ?");
    query.addBindValue(amount);
    query.addBindValue(userId);
    if (!query.exec()) {
        qDebug() << "rechargeBalance 失败：" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

QList<UserInfo> Database::getAllUsers(const QString &phoneKeyword)
{
    QList<UserInfo> result;
    QSqlQuery query(currentThreadDb());
    if (phoneKeyword.isEmpty()) {
        query.prepare("select id, phone, nickname, avatar_path, balance, status, created_at "
                       "from users order by id");
    } else {
        query.prepare("select id, phone, nickname, avatar_path, balance, status, created_at "
                       "from users where phone like ? order by id");
        query.addBindValue("%" + phoneKeyword + "%");
    }
    if (!query.exec()) {
        qDebug() << "getAllUsers 失败：" << query.lastError().text();
        return result;
    }
    while (query.next()) {
        UserInfo u;
        u.id = query.value(0).toInt();
        u.phone = query.value(1).toString();
        u.nickname = query.value(2).toString();
        u.avatarPath = query.value(3).toString();
        u.balance = query.value(4).toDouble();
        u.status = query.value(5).toString();
        u.createdAt = query.value(6).toString();
        result.append(u);
    }
    return result;
}

bool Database::setUserStatus(int userId, const QString &status)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("update users set status = ? where id = ?");
    query.addBindValue(status);
    query.addBindValue(userId);
    if (!query.exec()) {
        qDebug() << "setUserStatus 失败：" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}
