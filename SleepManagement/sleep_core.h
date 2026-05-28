#pragma once

//#include<bits/stdc++.h>
#include<iostream>
#include<stdlib.h>
#include<vector>
//加入Qt的字符串头文件
#include <QString>
#include <sstream>
#include <fstream>


//发现有一些有意义的变量，我们可以在顶部定义并控制
//比如判断是否熬夜的区间
#define oversleep_rewardScore 1
#define daysleep_rewardScore 1
#define daysleep_judgethreshold 15
int stayupBegin = 23;
int stayupEnd = 8;
int generalSleep_hour; // 一般入睡时间
int generalWake_hour;
using namespace std;

class SleepData{
protected:
    int Sleep_hour; //睡下的小时，应当用24h计数法？默认为25，防止熬大夜的时候没有数据
    int Sleep_min; //睡下的分钟
    int Wake_hour;
    int Wake_min;
    int Day_sleep; //午觉之类的睡眠时间（分钟）？
    int Exercise_time; //锻炼时间？
    int Sit_time; //坐的总时间？
    int UsualSleep_hour;
public:
    bool stayUp = false;
    bool oversleep = false;
    bool catchupOnSleep = false;
    bool noNightSleep = false;
    friend class WeeklyTracker;
    friend class SleepJsonExporter;
    SleepData(int d1,int d2,int d3,int d4, int d5,int d6,int d7){
        Sleep_hour=d1;
        Sleep_min=d2;
        Wake_hour=d3;
        Wake_min=d4;
        Day_sleep=d5;
        Exercise_time=d6;
        Sit_time=d7;
    }
};
class SleepAnalyzer : public SleepData {//自动统计每日睡眠时长、一周规律及熬夜天数
public:
    SleepAnalyzer(int d1 = -1, int d2 = -1, int d3 = -1, int d4 = -1, int d5 = 0, int d6 = 0, int d7 = 0)
        : SleepData(d1, d2, d3, d4, d5, d6, d7) {
        if (d1 == -1) noNightSleep = true;
    }

    int calculateNightSleep() {  //计算晚上睡眠时间：[一般睡觉时间提前2h, 一般起床时间延后2h]之间的时间段
        if (noNightSleep) return 0; // 如果没有记录睡眠时间，则晚上睡眠时间为0

        int start = Sleep_hour * 60 + Sleep_min;  //计算睡眠时间点在一天中是第几分钟
        int end = Wake_hour * 60 + Wake_min;  //计算起床时间点在一天中是第几分钟
        //此时用户昨晚肯定是睡了觉的，也就是入睡时间不会晚于其一般起床时间。只会出现早睡（早于其一般入睡时间）与正常睡；以及睡懒觉和正常起
        //早睡
        //if (start <= (generalSleep_hour - 2) * 60) start = (generalSleep_hour - 2) * 60;
        //晚起床
        if (end >= (generalWake_hour + 2) * 60) oversleep = true;
        if (end < start) end += 24 * 60;   //如果睡眠时间（如23:00）晚于起床时间（如8:00），认为二者不在同一天，将8+24 = 32
        return end - start; //作差得到睡眠总分钟数
    }

    //一个返回值是QString的显示睡眠时长的函数
    QString ShowSleepTime(const int &t){
        int h = t / 60;
        int m = t % 60;
        return QString("%1小时 %2分钟").arg(h).arg(m);
    }

    int getTotalSleep() {
        return calculateNightSleep() + Day_sleep;
    }


    bool isStayUpLate() { //判断是否熬夜，如果在设定的0点和6点间入睡就算作熬夜
        //这个是否熬夜和是否补觉的判断标准，初始让用户自己输入，后面可以根据记录的数据取平均作为建议
        if (Sleep_hour >= stayupBegin && Sleep_hour < stayupEnd){
            stayUp = true;
            return true;
        }
        else if (noNightSleep){
            stayUp = true;
            return true; //没睡也认为是熬夜
        }
        return false;
    }
    int getEnoughSleepScore(){
        int nightsleep = calculateNightSleep();
        //总之我觉得后面可以把晚上睡眠和白天睡眠分开考虑，晚上不睡白天昏昏欲睡也不好，但总比不睡好x
        if (noNightSleep) return -5; // 熬穿
        else if(oversleep) return 0; //睡懒觉，后面再评价：如果睡眠分小于一定数目，睡懒觉有加分（补觉）；否则不加分
        else if(nightsleep<360) return 1; //小于6h
        else if(nightsleep<420) return 2; //6-7h
        else if(nightsleep<480) return 3; //7-8h
        else if(nightsleep<=540) return 3; //8-9h
        else if(nightsleep<=600) return 2; //9-10h
        else return 1; //大于10h且不算睡懒觉；睡太长也不好
    }
    void displayReport() {
        int total = getTotalSleep();
        int night = calculateNightSleep();
        cout << "--- 每日睡眠报告 ---" << endl;
        cout << "总睡眠时长: " << total / 60 << "小时 " << total % 60 << "分钟" << endl;
        cout << "总夜间睡眠时长: " << night / 60 << "小时 " << night % 60 << "分钟" << endl;
    }
};

class WeeklyTracker {
protected:
    vector<SleepAnalyzer> weekData; //sleepData是基类，Analyzer是派生类，这个向量的名字是不是有混淆性x
    int totalStayUp = 0; //总熬夜天数。如isStayUpdate里注释所说，或许还应该来一个总“日夜颠倒”天数
    double weeklySum = 0;
    int timescore=0;
    int totalOverSleep = 0;
    int totalCatchupOnSleep = 0;
public:
    void addDay(SleepAnalyzer &sa) {
        weekData.push_back(sa);
    }
    // 新增：返回 Qt 字符串格式的周评语
    QString getCommentString(const int &n) {
        if(n >= 16) return "你这样的睡眠不可特意去求！";
        else if(n >= 10 && n <= 15) return "不错的睡眠，算挺健康的大学生了qaq";
        else if(n >= 4 && n <= 9) return "xs你是赶早八的大学生吗";
        else if(n >= -6 && n <= 3) return "攻城狮劝你别炼丹了";
        else return "不是哥们，睡眠时长这一块咱上点心吧";
    }
    // 新增：生成供 Qt 界面显示的本地周报
    QString generateLocalReport() {
        int localTotalStayUp = 0;
        int localTotalOverSleep = 0;
        int localTotalCatchup = 0;
        int localTimeScore = 0;
        double localWeeklySum = 0;

        for (auto &day : weekData) {
            localWeeklySum += day.getTotalSleep();

            if (day.isStayUpLate()) {
                localTotalStayUp++;
                if (day.Day_sleep > daysleep_judgethreshold) {
                    localTimeScore += daysleep_rewardScore;
                }
            }
            if (day.oversleep) {
                localTotalOverSleep++;
                if (localTotalOverSleep <= localTotalStayUp + 2) {
                    day.catchupOnSleep = true;
                    localTotalCatchup++;
                    localTimeScore += oversleep_rewardScore;
                }
            }
            localTimeScore += day.getEnoughSleepScore();
        }

        QString report = "=== 🏠 本地核心算法周报 ===\n\n";
        report += QString("🛌 本周熬夜天数: %1 天\n").arg(localTotalStayUp);
        report += QString("💤 本周睡懒觉天数: %1 天 (其中有效补觉 %2 天)\n").arg(localTotalOverSleep).arg(localTotalCatchup);

        int avgSleepMin = weekData.empty() ? 0 : (localWeeklySum / weekData.size());
        report += QString("📊 日均睡眠时长: %1 小时 %2 分钟\n").arg(avgSleepMin / 60).arg(avgSleepMin % 60);
        report += QString("💯 本周综合睡眠分: %1\n\n").arg(localTimeScore);
        report += "💡 算法总评：\n" + getCommentString(localTimeScore);

        return report;
    }
    /*void PrintComment(const int &n){  //n是一周7天所有的getEnoughSleepScore返回值的累加 参数是睡眠分x
        //这可以作为一个简短评价，比如作为整个睡眠评价页面的标题？（类似SBTI那样）剩下的部分是模型生成的分析报告
        //不过现在已经可以了x
        //或许加入睡懒觉天数的判断和记录后，可以另开一个if-else分支关注这个点，加一些特定条件下的称号
        if(n>=16)cout<<"你这样的睡眠不可特意去求！"; //或许睡眠分也可以打出来
        else if(n>=10&&n<=15)cout<<"不错的睡眠,算挺健康的大学生了qaq";
        else if(n>=4&&n<=9)cout<<"xs你是赶早八的大学生吗";
        else if(n>=-6&&n<=3)cout<<"攻城狮劝你别炼丹了";
        else cout<<"不是哥们,睡眠时长这一块咱上点心吧";
    }*/

    /*void showWeeklySummary() {
        //现在只评价了睡眠时间，以后要加入锻炼、久坐时间的评价？算了喂给大模型评价吧
        for (auto &day : weekData) {
            weeklySum += day.getTotalSleep();

            //如果熬了夜但是当天有午觉补回来，则睡眠分有补偿
            if (day.stayUp){
                totalStayUp++;
                if (day.Day_sleep > daysleep_judgethreshold){
                    timescore += daysleep_rewardScore;
                }
            }
            // 如果熬了夜，那么睡懒觉视为补觉，应当有加分；否则就是纯懒，没有加分
            if (day.oversleep){
                totalOverSleep++;
                if (totalOverSleep <= totalStayUp + 2){
                    day.catchupOnSleep = true;
                    totalCatchupOnSleep++;
                    timescore += oversleep_rewardScore;
                }
            }
            timescore+=day.getEnoughSleepScore();
        }
        cout << "\n=== 一周数据汇总 ===" << endl;
        cout << "本周熬夜天数: " << totalStayUp << " 天" << endl;
        cout << "本周睡懒觉天数: " << totalOverSleep << " 天，补觉天数为：" << totalCatchupOnSleep << " 天" << endl;
        cout << "日均睡眠时长: " << (weeklySum / weekData.size()) / 60 << " 小时" << endl;
        cout<<"本周的睡眠分: "<<timescore<<endl;
        PrintComment(timescore);
    }*/
};


class SleepJsonExporter {
public:
    /**
     * @brief 接收一个装好了当天数据的 SleepAnalyzer 对象，以及一个你从前端传进来的日期字符串（比如 "2025-01-15"），返回标准的 JSON 字符串
     * @param sa        当天的 SleepAnalyzer 对象（已填入数据）
     * @param dateStr   日期字符串，格式建议 "YYYY-MM-DD"，由前端传入
     * @return          标准 JSON 字符串
     */
    static std::string toJsonString(const SleepAnalyzer& sa, const std::string& dateStr) {
        // SleepAnalyzer 继承自 SleepData，成员是 protected，
        // 通过 friend 或 getter 访问；这里我们用计算接口绕过
        SleepAnalyzer copy = sa; // 拷贝一份用于调用非const方法

        int nightSleep  = copy.calculateNightSleep();   // 夜间睡眠（分钟）
        int totalSleep  = copy.getTotalSleep();          // 总睡眠（分钟）
        bool stayUp     = copy.isStayUpLate();           // 是否熬夜
        int sleepScore  = copy.getEnoughSleepScore();    // 睡眠评分

        std::ostringstream oss;//ostringstream 把所有数据拼成 JSON 格式的字符串
        oss << "{\n"
            << "  \"date\": \""         << escapeJson(dateStr) << "\",\n"
            << "  \"sleep_hour\": "     << sa.Sleep_hour       << ",\n"
            << "  \"sleep_min\": "      << sa.Sleep_min        << ",\n"
            << "  \"wake_hour\": "      << sa.Wake_hour        << ",\n"
            << "  \"wake_min\": "       << sa.Wake_min         << ",\n"
            << "  \"day_sleep_min\": "  << sa.Day_sleep        << ",\n"
            << "  \"exercise_min\": "   << sa.Exercise_time    << ",\n"
            << "  \"sit_min\": "        << sa.Sit_time         << ",\n"
            << "  \"night_sleep_min\": "<< nightSleep          << ",\n"
            << "  \"total_sleep_min\": "<< totalSleep          << ",\n"
            << "  \"stay_up_late\": "   << (stayUp ? "true" : "false") << ",\n"
            << "  \"oversleep\": "       << (copy.oversleep ? "true" : "false")     << ",\n"
            << "  \"no_night_sleep\": "  << (copy.noNightSleep ? "true" : "false")  << ",\n"
            << "  \"sleep_score\": "    << sleepScore          << "\n"
            << "}";
        return oss.str();
    }

    /**
     * @brief 将 JSON 字符串写入文件（可选，便于本地存档）
     * @param jsonStr   由 toJsonString() 生成的字符串
     * @param filePath  目标文件路径，如 "2025-01-01.json"
     * @return          写入成功返回 true
     */
    static bool saveToFile(const std::string& jsonStr, const std::string& filePath) {
        std::ofstream ofs(filePath);
        if (!ofs.is_open()) return false;
        ofs << jsonStr;
        return true;
    }

private:
    // 对字符串中的特殊字符做基础转义，防止 JSON 格式破坏
    static std::string escapeJson(const std::string& s) {
        std::ostringstream oss;
        for (char c : s) {
            switch (c) {
                case '"':  oss << "\\\""; break;
                case '\\': oss << "\\\\"; break;
                case '\n': oss << "\\n";  break;
                case '\r': oss << "\\r";  break;
                case '\t': oss << "\\t";  break;
                default:   oss << c;
            }
        }
        return oss.str();
    }
};//重新上传一下
