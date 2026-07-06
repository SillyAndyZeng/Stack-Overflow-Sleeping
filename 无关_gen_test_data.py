#!/usr/bin/env python3
"""生成测试数据（用过去日期，避开setMaximumDate限制）"""

import json, os, datetime
from pathlib import Path

data_dir = os.path.expanduser("~/Library/Application Support/SleepManagement")
Path(data_dir).mkdir(parents=True, exist_ok=True)

today = datetime.date.today()

# 7种数据，从今天往前推7天
profiles = [
    # (入睡时,入睡分,起床时,起床分,午睡,运动,久坐,熬夜?,懒觉?,通宵?,评分)
    # 0天前（今天）：通宵-红
    {"sleep_hour":-1,"sleep_min":-1,"wake_hour":-1,"wake_min":-1,
     "day_sleep_min":0,"exercise_min":0,"sit_min":960,
     "stay_up_late":True,"oversleep":False,"no_night_sleep":True,"sleep_score":-5},
    # 1天前：良好-绿
    {"sleep_hour":22,"sleep_min":30,"wake_hour":7,"wake_min":0,
     "day_sleep_min":20,"exercise_min":45,"sit_min":300,
     "stay_up_late":False,"oversleep":False,"no_night_sleep":False,"sleep_score":3},
    # 2天前：普通-蓝
    {"sleep_hour":23,"sleep_min":15,"wake_hour":7,"wake_min":30,
     "day_sleep_min":0,"exercise_min":0,"sit_min":480,
     "stay_up_late":False,"oversleep":False,"no_night_sleep":False,"sleep_score":2},
    # 3天前：熬夜-黄
    {"sleep_hour":2,"sleep_min":30,"wake_hour":9,"wake_min":0,
     "day_sleep_min":60,"exercise_min":30,"sit_min":360,
     "stay_up_late":True,"oversleep":False,"no_night_sleep":False,"sleep_score":1},
    # 4天前：久坐超标-橙
    {"sleep_hour":23,"sleep_min":0,"wake_hour":7,"wake_min":0,
     "day_sleep_min":0,"exercise_min":20,"sit_min":720,
     "stay_up_late":False,"oversleep":False,"no_night_sleep":False,"sleep_score":2},
    # 5天前：熬夜+久坐-深橙
    {"sleep_hour":3,"sleep_min":0,"wake_hour":11,"wake_min":0,
     "day_sleep_min":120,"exercise_min":0,"sit_min":600,
     "stay_up_late":True,"oversleep":True,"no_night_sleep":False,"sleep_score":1},
    # 6天前：睡懒觉-紫
    {"sleep_hour":23,"sleep_min":0,"wake_hour":12,"wake_min":30,
     "day_sleep_min":30,"exercise_min":30,"sit_min":200,
     "stay_up_late":False,"oversleep":True,"no_night_sleep":False,"sleep_score":1},
]

for i, pf in enumerate(profiles):
    d = today - datetime.timedelta(days=i)
    # 算 night_sleep_min
    if pf["no_night_sleep"]:
        night = 0
    else:
        s = pf["sleep_hour"]*60 + pf["sleep_min"]
        w = pf["wake_hour"]*60 + pf["wake_min"]
        if w < s:
            w += 24*60
        night = w - s
    pf["date"] = d.isoformat()
    pf["night_sleep_min"] = night
    pf["total_sleep_min"] = night + pf["day_sleep_min"]

    filepath = os.path.join(data_dir, f"{d.isoformat()}.json")
    with open(filepath, "w", encoding="utf-8") as f:
        json.dump(pf, f, indent=2, ensure_ascii=False)

print(f"已生成 7 天测试数据（{profiles[-1]['date']} ~ {profiles[0]['date']}）")
