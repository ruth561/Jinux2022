#pragma once

namespace logging
{
    // DEBUGが最も重要度が低く、CRITICALが最も重要度が高い。
    enum LoggingLevel
    {
        kDEBUG    = 0, // デバッグ
        kINFO     = 1, // 情報
        kWARNING  = 2, // 警告
        kERROR    = 3 // エラー
    };

    class Logger
    {
    public:
        Logger() {}

        // ログレベルのしきい値を設定する。
        void set_level(LoggingLevel level) {
            level_ = level;
        }

        LoggingLevel current_level() {
            return level_;
        }

        int debug(const char *format, ...);    // levelがDEBUG以上のときのみ出力。
        int info(const char *format, ...);     // levelがINFO以上のときのみ出力。
        int warning(const char *format, ...);  // levelがWARNING以上のときのみ出力。
        int error(const char *format, ...);    // levelがERROR以上のときのみ出力。

    private:
        LoggingLevel level_;
    };


}
