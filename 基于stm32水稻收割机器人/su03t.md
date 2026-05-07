su03t的指令是

wakeup_uni (唤醒时)：填入 AA 00 （STM32收到后可以亮个灯，表示“我听到了”）

MoveForward (向前开)：填入 AA 01

MoveBackward (向后倒)：填入 AA 02

TurnLeft (向左转)：填入 AA 03

TurnRight (向右转)：填入 AA 04

StopMove (停车)：填入 AA 05

StartCut (开始收割)：填入 AA 11

StopCut (停止收割)：填入 AA 12

StartThresh (开始脱粒)：填入 AA 13

ReportStatus (汇报状态)：填入 AA 22

EmergencyStop (紧急停止)：填入 AA 23

AllStart（开启全自动无人收割|执行一键收割）：填入AA FF

波特率是115200