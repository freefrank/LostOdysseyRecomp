#pragma once
#include <cstdint>
#include <string_view>
namespace settings
{
// English and Traditional Chinese are provided at the call site.
inline const wchar_t *Translate(uint32_t language, const wchar_t *en, const wchar_t *traditional)
{
    if (language == 0)
        return en;
    if (language == 1)
        return traditional;
    struct Entry
    {
        const wchar_t *key, *japanese, *korean, *simplified;
    };
    static constexpr Entry entries[] = {
        {L"On", L"オン", L"켜기", L"开"},
        {L"Off", L"オフ", L"끄기", L"关"},
        {L"Fast", L"速い", L"빠르게", L"快"},
        {L"Normal", L"普通", L"보통", L"正常"},
        {L"Slow", L"遅い", L"느리게", L"慢"},
        {L"Text speed", L"文字速度", L"텍스트 속도", L"文字速度"},
        {L"Captions", L"字幕", L"자막", L"字幕"},
        {L"Remember battle cursor", L"戦闘カーソルを記憶", L"전투 커서 기억", L"记住战斗光标"},
        {L"Automatic back-row input", L"後列の自動入力", L"후열 자동 입력", L"后排自动输入"},
        {L"Invert camera vertically", L"カメラ上下反転", L"카메라 상하 반전", L"反转镜头上下"},
        {L"Invert camera horizontally", L"カメラ左右反転", L"카메라 좌우 반전", L"反转镜头左右"},
        {L"Confirmation button", L"決定ボタン", L"확인 버튼", L"确认按键"},
        {L"Restore game defaults", L"ゲーム設定を初期化", L"게임 설정 초기화", L"恢复游戏默认设置"},
        {L"Restore", L"初期化", L"초기화", L"恢复"},
        {L"Voice language", L"音声言語", L"음성 언어", L"语音语言"},
        {L"Music", L"音楽の音量", L"음악 음량", L"音乐音量"},
        {L"Sound effects", L"効果音の音量", L"효과음 음량", L"音效音量"},
        {L"Windowed", L"ウィンドウ", L"창 모드", L"窗口"},
        {L"Borderless fullscreen", L"ボーダーレス全画面", L"테두리 없는 전체 화면", L"无边框全屏"},
        {L"Exclusive fullscreen", L"排他全画面", L"독점 전체 화면", L"独占全屏"},
        {L"Display mode", L"表示モード", L"화면 모드", L"显示模式"},
        {L"Output resolution", L"出力解像度", L"출력 해상도", L"输出分辨率"},
        {L"Anti-aliasing", L"アンチエイリアス", L"안티앨리어싱", L"抗锯齿"},
        {L"DLSS", L"DLSS", L"DLSS", L"DLSS"},
        {L"Not implemented", L"未実装", L"미구현", L"尚未实现"},
        {L"Frame generation", L"フレーム生成", L"프레임 생성", L"帧生成"},
        {L"Brightness calibration", L"明るさ調整", L"밝기 조정", L"亮度校准"},
        {L"Open", L"開く", L"열기", L"打开"},
        {L"Apply display settings", L"表示設定を適用", L"화면 설정 적용", L"应用显示设置"},
        {L"Keep changes", L"変更を保存", L"변경 유지", L"保留更改"},
        {L"Apply", L"適用", L"적용", L"应用"},
        {L"Settings language", L"設定画面の言語", L"설정 화면 언어", L"设置界面语言"},
        {L"Game language", L"ゲームの言語", L"게임 언어", L"游戏语言"},
        {L"Save language settings", L"言語設定を保存", L"언어 설정 저장", L"保存语言设置"},
        {L"Save", L"保存", L"저장", L"保存"},
        {L"LB / RB: category     D-pad: select / change     A: select     B: back",
         L"LB / RB：タブ    方向キー：選択 / 変更    A：決定    B：戻る",
         L"LB / RB: 탭    방향키: 선택 / 변경    A: 확인    B: 뒤로",
         L"LB / RB：分类    方向键：选择 / 调整    A：确认    B：返回"},
        {L"LB / RB: category     D-pad: select / change     B: select     A: back",
         L"LB / RB：タブ    方向キー：選択 / 変更    B：決定    A：戻る",
         L"LB / RB: 탭    방향키: 선택 / 변경    B: 확인    A: 뒤로",
         L"LB / RB：分类    方向键：选择 / 调整    B：确认    A：返回"},
        {L"Game language takes effect after restarting. Requires matching language assets.",
         L"ゲームの言語は再起動後に適用されます。対応する言語データが必要です。",
         L"게임 언어는 재시작 후 적용됩니다. 해당 언어 리소스가 필요합니다.",
         L"游戏语言重启后生效，需要对应语言资源。"},
        {L"Scales the original game image to the output resolution. Borderless uses the desktop size.",
         L"元の映像を出力解像度に拡大します。ボーダーレスはデスクトップの解像度を使用します。",
         L"원본 화면을 출력 해상도로 조정합니다. 테두리 없는 모드는 바탕 화면 크기를 사용합니다.",
         L"将原始游戏画面缩放至输出分辨率；无边框模式使用桌面尺寸。"},
        {L"Keep changes? A: keep, B: revert. Reverting automatically in 15 seconds.",
         L"変更を保存しますか？ A：保存  B：元に戻す。15 秒後に自動で戻ります。",
         L"변경을 유지할까요? A: 유지  B: 복원. 15초 후 자동 복원됩니다.",
         L"保留显示更改？A：保留，B：还原。15 秒后自动还原。"},
        {L"Keep changes? B: keep, A: revert. Reverting automatically in 15 seconds.",
         L"変更を保存しますか？ B：保存  A：元に戻す。15 秒後に自動で戻ります。",
         L"변경을 유지할까요? B: 유지  A: 복원. 15초 후 자동 복원됩니다.",
         L"保留显示更改？B：保留，A：还原。15 秒后自动还原。"},
        {L"Display settings reverted.", L"表示設定を元に戻しました。", L"화면 설정을 복원했습니다.",
         L"显示设置已还原。"},
        {L"Display mode unavailable; previous settings restored.",
         L"表示モードを使用できません。以前の設定に戻しました。",
         L"사용할 수 없는 화면 모드입니다. 이전 설정을 복원했습니다.", L"显示模式不可用，已还原之前的设置。"},
        {L"Display settings saved.", L"表示設定を保存しました。", L"화면 설정을 저장했습니다.", L"显示设置已保存。"},
        {L"Could not save settings.", L"設定を保存できませんでした。", L"설정을 저장하지 못했습니다.",
         L"无法保存设置。"},
        {L"Game defaults restored.", L"ゲーム設定を初期化しました。", L"게임 설정을 초기화했습니다.",
         L"游戏默认设置已恢复。"},
        {L"Saved. Restart the game to apply text language.", L"保存しました。テキスト言語の変更には再起動が必要です。",
         L"저장했습니다. 텍스트 언어를 적용하려면 게임을 재시작하세요.", L"已保存。重启游戏后应用文本语言。"},
        {L"Settings", L"設定", L"설정", L"设置"},
        {L"Gameplay", L"ゲーム", L"게임", L"游戏"},
        {L"Audio", L"サウンド", L"오디오", L"声音"},
        {L"Graphics", L"グラフィックス", L"그래픽", L"图像"},
        {L"Language", L"言語", L"언어", L"语言"},
    };
    for (const auto &entry : entries)
        if (std::wstring_view(en) == entry.key)
            return language == 2 ? entry.japanese : language == 3 ? entry.korean : entry.simplified;
    return en;
}
} // namespace settings
