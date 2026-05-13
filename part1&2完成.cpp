#include<bits/stdc++.h>


//发现有一些有意义的变量，我们可以在顶部定义并控制
//比如判断是否熬夜的区间 
#define stayupBegin = 0;
#define stayupEnd = 6;
using namespace std;

class SleepData{
protected:
    int Sleep_hour; //睡下的小时，应当用24h计数法？
    int Sleep_min; //睡下的分钟
    int Wake_hour;
    int Wake_min;
    int Day_sleep; //午觉之类的睡眠时间（分钟）？
    int Exercise_time; //锻炼时间？
    int Sit_time; //坐的总时间？
public:
    SleepData(int d1,int d2,int d3,int d4,int d5,int d6,int d7){
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
    SleepAnalyzer(int d1, int d2, int d3, int d4, int d5, int d6, int d7)
        : SleepData(d1, d2, d3, d4, d5, d6, d7) {}

    int calculateNightSleep() {  //计算睡眠时间
        int start = Sleep_hour * 60 + Sleep_min;  //计算睡眠时间点在一天中是第几分钟
        int end = Wake_hour * 60 + Wake_min;  //计算起床时间点在一天中是第几分钟
        if (end < start) end += 24 * 60;   //如果睡眠时间（如23:00）晚于起床时间（如8:00），认为二者不在同一天，将8+24 = 32
        return end - start; //作差得到睡眠总分钟数
    }

    int getTotalSleep() {
        return calculateNightSleep() + Day_sleep;
    }

    bool isStayUpLate() { //判断是否熬夜，如果在0点和6点间入睡就算作熬夜
        //其实感觉有点问题，比如我可以熬通宵，没有入睡时间，或者入睡时间在上午，然后睡到下午或者晚上
        //所以入睡时间可以整一个默认值，如果没有修改代表没有入睡；而且6点感觉还是早了x
        //如果出现白天补觉的情况，或许可以加一个函数判断入睡时间是否在白天，起床时间是否远远晚于一般起床时间，这样不但视为熬夜，而且视为白天补觉
        //如果入睡时间在凌晨或者早上，但是起床时间仍然是早上，则仍视为熬夜，不视为补觉
        //这个是否熬夜和是否补觉的判断标准，我觉得可以整一个generalSleep_hour和generalWake_hour，初始让用户自己输入，后面根据记录的数据取平均
        if (Sleep_hour >= stayupBegin && Sleep_hour < stayupEnd) return true;
        return false;
    }
    int EnoughSleep(){ //感觉可以改为getEnoughSleepScore
        //总之我觉得后面可以把晚上睡眠和白天睡眠分开考虑，晚上不睡白天昏昏欲睡也不好，但总比不睡好x
        //午睡另说，当不存在熬夜时，白天的睡觉就不视为补觉了
        if(getTotalSleep()<360) return -3; //小于6h
        else if(getTotalSleep()>=360&&getTotalSleep()<420) return -1; //6-7h
        else if(getTotalSleep()>=420&&getTotalSleep()<480) return 2; //7-8h
        else if(getTotalSleep()>=480&&getTotalSleep()<=540) return 3; //8-9h
        else  return -1; //>9h，认为和6-7h同一级别嘛（）
    }
    void displayReport() {
        int total = getTotalSleep();
        cout << "--- 每日睡眠报告 ---" << endl;
        cout << "总睡眠时长: " << total / 60 << "小时 " << total % 60 << "分钟" << endl;
    }
};
void PrintTimeScore(int n){  //n是一周7天所有的EnoughSleep()返回值的累加 另外函数名要不改成PrintSleepComment？毕竟打印的也不是睡眠分 反而参数是睡眠分x
    //这可以作为一个简短评价，比如作为整个睡眠评价页面的标题？（类似SBTI那样）剩下的部分是模型生成的分析报告
    //不过现在已经可以了x
    //或许加入白天睡觉（日夜颠倒）天数的判断和记录后，可以另开一个if-else分支关注这个点，加一些特定条件下的称号
    if(n>=16)cout<<"你这样的睡眠不可特意去求！"; //或许睡眠分也可以打出来
    else if(n>=10&&n<=15)cout<<"不错的睡眠,算挺健康的大学生了qaq";
    else if(n>=4&&n<=9)cout<<"xs你是赶早八的大学生吗";
    else if(n>=-6&&n<=3)cout<<"攻城狮劝你别炼丹了";
    else cout<<"不是哥们,睡眠时长这一块咱上点心吧";
} //另：何不将这个函数作为WeeklyTracer的成员函数x

class WeeklyTracker {
private:
    vector<SleepAnalyzer> weekData; //sleepData是基类，Analyzer是派生类，这个向量的名字是不是有混淆性x
public:
    void addDay(SleepAnalyzer sa) {
        weekData.push_back(sa);
    }

    void showWeeklySummary() {
        int totalStayUp = 0; //总熬夜天数。如isStayUpdate里注释所说，或许还应该来一个总“日夜颠倒”天数
        double weeklyAvg = 0;
        int timescore=0;
        for (auto &day : weekData) {
            if (day.isStayUpLate()) totalStayUp++; //
            weeklyAvg += day.getTotalSleep();
            timescore+=day.EnoughSleep();
        }
        cout << "\n=== 一周数据汇总 ===" << endl;
        cout << "本周熬夜天数: " << totalStayUp << " 天" << endl;
        cout << "日均睡眠时长: " << (weeklyAvg / weekData.size()) / 60 << " 小时" << endl;
        cout<<"本周的睡眠分: "<<timescore<<endl;
        PrintTimeScore(timescore);
    }
};
int main(){
    int sleephour,sleepmin,wakehour,wakemin,daysleep,exertime,sittime;
    WeeklyTracker myWeek;
    for(int i = 0; i < 7; i++) {
        while (true) { 
            char confirm;
            cout << "请输入本周周 " << i+1 << " 数据 (入睡h m, 起床h m, 午睡, 运动, 久坐)(空格分隔,24小时制): " << endl;
            cout << "入睡时间 (时 分): "; cin >> sleephour >> sleepmin;
            cout << "起床时间 (时 分): "; cin >>  wakehour >> wakemin;
            cout << "午睡, 运动, 久坐时长 (min): "; cin >> daysleep >> exertime >> sittime;

            // 回显数据供用户核对
            cout << "\n您输入的数据如下" << endl;
            printf("入睡：%02d:%02d,起床：%02d:%02d\n", sleephour, sleepmin, wakehour, wakemin);
            printf("午睡：%dmin,运动：%dmin,久坐：%dmin\n", daysleep, exertime, sittime);
            
            cout << "确认无误吗？(输入 y 确认，输入 n 重新填写): ";cin >> confirm;

            if (confirm == 'y' || confirm == 'Y') {
                SleepAnalyzer today(sleephour, sleepmin, wakehour, wakemin, daysleep, exertime, sittime);
                today.displayReport();
                myWeek.addDay(today);
                break; // 用户确认，跳出循环
            }
            cout << "-> 检测到输入有误，请重新填写。" << endl;
        }
    }
    myWeek.showWeeklySummary();
}