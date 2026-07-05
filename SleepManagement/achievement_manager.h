#ifndef ACHIEVEMENT_MANAGER_H
#define ACHIEVEMENT_MANAGER_H

#pragma once
#include <QDate>
#include <QString>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

// 纯逻辑类，不依赖任何 UI，只读本地 JSON 文件
class AchievementManager {
public:
    explicit AchievementManager(const QString &dataDir) : m_dir(dataDir) {}

    // 从昨天开始向前数，近三个月内连续有打卡记录的天数
    int consecutiveCheckIn() const {
        int count = 0;
        for (int i = 1; i <= 90; i++) {
            QString path = filePath(QDate::currentDate().addDays(-i));
            if (!QFile::exists(path)) break;
            count++;
        }
        return count;
    }

    // 从今天向前数，近三个月内连续"早睡"（入睡早于 23:00）的天数
    int consecutiveEarlySleep() const {
        int count = 0;
        for (int i = 1; i <= 90; i++) {
            QJsonObject obj = readRecord(QDate::currentDate().addDays(-i));
            if (obj.isEmpty()) break;
            if (obj["sleep_hour"].toInt(25) >= 23) break;
            count++;
        }
        return count;
    }

    // 【新增核心算法】同时计算“完美睡眠(=3分)”和“良好睡眠(>=2分)”的连续天数
    void getSleepScoreStreaks(int &perfectStreak, int &goodStreak) const {
        perfectStreak = 0;
        goodStreak = 0;
        bool perfectBroken = false;
        bool goodBroken = false;

        for (int i = 1; i <= 90; i++) {
            QJsonObject obj = readRecord(QDate::currentDate().addDays(-i));
            if (obj.isEmpty()) break; // 没打卡，全部中断

            int score = obj["sleep_score"].toInt(0);

            // 计算连续 >= 2 分
            if (!goodBroken) {
                if (score >= 2) goodStreak++;
                else goodBroken = true;
            }

            // 计算连续 = 3 分
            if (!perfectBroken) {
                if (score == 3) perfectStreak++;
                else perfectBroken = true;
            }

            // 如果两个都断了，就没必要继续往前找了
            if (perfectBroken && goodBroken) break;
        }
    }

    int perfectSleepDays() const {
        int p = 0, g = 0;
        getSleepScoreStreaks(p, g);
        return p;
    }

    int goodSleepDays() const {
        int p = 0, g = 0;
        getSleepScoreStreaks(p, g);
        return g;
    }

    // 返回当前解锁的所有徽章拼成的一行文字（空字符串 = 无成就）
    QString currentBadge() const {
        int ci = consecutiveCheckIn();
        int es = consecutiveEarlySleep();
        int ps = perfectSleepDays(); // 连续 3 分天数
        int gs = goodSleepDays();    // 连续 >= 2 分天数

        QStringList badges;

        // 1. 基础打卡成就
        if      (ci >= 30) badges << "🏅 月度规律作息达人";
        else if (ci >= 7)  badges << "⭐ 七日连续打卡";

        // 2. 早睡成就
        if      (es >= 7)  badges << "🌙 连续早睡一周";
        else if (es >= 3)  badges << "✨ 早睡初心者";

        // 3. 💡【新增】睡眠质量成就
        // 逻辑精髓：如果 ps >= 7，它会自动覆盖 gs >= 7 的成就不显示（因为全拿了 3 分）。
        // 如果 gs >= 7 但 ps 没满 7，则说明“大于等于2但不完全为3”，就会触发“不错的尝试”。
        if (ps >= 7) {
            badges << "👑 人类高级睡眠";
        } else if (gs >= 7) {
            badges << "👍 不错的尝试";
            // 补充判定：虽然 7 天不全是 3 分，但如果最近 3 天都是 3 分，也该给 3 天的完美徽章
            if (ps >= 3) badges << "🌟 优质睡眠初尝试";
        } else if (ps >= 3) {
            badges << "🌟 优质睡眠初尝试";
        } else if (gs >= 3) {
            badges << "🌱 很有潜力";
        }

        return badges.join("  ");
    }

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