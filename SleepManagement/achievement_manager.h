#ifndef ACHIEVEMENT_MANAGER_H
#define ACHIEVEMENT_MANAGER_H

#pragma once
#include <QDate>
#include <QString>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList> //

// 纯逻辑类，不依赖任何 UI，只读本地 JSON 文件
// 使用方式：AchievementManager am(dataDir()); am.currentBadge();
class AchievementManager {
public:
    explicit AchievementManager(const QString &dataDir) : m_dir(dataDir) {}

    // 从今天向前数，连续有打卡记录的天数
    int consecutiveCheckIn() const {
        int count = 0;
        for (int i = 0; i < 90; i++) {
            QString path = filePath(QDate::currentDate().addDays(-i));
            if (!QFile::exists(path)) break;
            count++;
        }
        return count;
    }

    // 从今天向前数，连续"早睡"（入睡早于 23:00）的天数
    // 依赖 sleep_hour 字段，不依赖尚未写入的布尔字段
    int consecutiveEarlySleep() const {
        int count = 0;
        for (int i = 0; i < 90; i++) {
            QJsonObject obj = readRecord(QDate::currentDate().addDays(-i));
            if (obj.isEmpty()) break;                       // 该天无记录，中断
            if (obj["sleep_hour"].toInt(25) >= 23) break;  // 不算早睡，中断
            count++;
        }
        return count;
    }

    // 返回当前解锁的所有徽章拼成的一行文字（空字符串 = 无成就）
    QString currentBadge() const {
        int ci = consecutiveCheckIn();
        int es = consecutiveEarlySleep();

        QStringList badges;
        if      (ci >= 30) badges << "🏅 月度规律作息达人";
        else if (ci >= 7)  badges << "⭐ 七日连续打卡";

        if      (es >= 7)  badges << "🌙 连续早睡一周";
        else if (es >= 3)  badges << "✨ 早睡初心者";

        return badges.join("  ");
    }

    // 返回连续打卡天数（供界面直接显示）
    int checkInDays()   const { return consecutiveCheckIn(); }
    int earlySleepDays() const { return consecutiveEarlySleep(); }

private:
    QString m_dir;

    QString filePath(const QDate &date) const {
        return m_dir + "/" + date.toString("yyyy-MM-dd") + ".json";
    }

    QJsonObject readRecord(const QDate &date) const {
        QFile file(filePath(date));
        if (!file.open(QIODevice::ReadOnly)) return {};
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        return doc.isObject() ? doc.object() : QJsonObject{};
    }
};



#endif // ACHIEVEMENT_MANAGER_H
