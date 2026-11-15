#include "i18n/Translations.h"

#include <atomic>
#include <string>
#include <unordered_map>

namespace curvable::i18n
{

namespace
{
    std::atomic<Language> current { Language::english };

    juce::ListenerList<Listener>& listeners()
    {
        static juce::ListenerList<Listener> list;
        return list;
    }

    // 顺序：英语（原文）、简体中文、日语、韩语、西班牙语。
    // 规则：不用间隔符（如"·"），补充说明用括号；{0} {1} 是参数，每种语言都要保留。
    const std::vector<Entry> table {
        // ---- 吸附、弯音范围、工具
        { { "Bar", "小节", "小節", "마디", "Compás" } },
        { { "1/8 triplet", "1/8 三连音", "1/8 3連符", "1/8 셋잇단", "1/8 tresillo" } },
        { { "1/16 triplet", "1/16 三连音", "1/16 3連符", "1/16 셋잇단", "1/16 tresillo" } },
        { { "None", "无", "なし", "없음", "Ninguno" } },
        { { "Custom", "自定义", "カスタム", "사용자 지정", "Personalizado" } },
        { { "Snap  {0}", "吸附  {0}", "スナップ  {0}", "스냅  {0}", "Ajuste  {0}" } },
        { { "Bend \xc2\xb1{0}", "弯音 \xc2\xb1{0}", "ベンド \xc2\xb1{0}", "벤드 \xc2\xb1{0}", "Bend \xc2\xb1{0}" } },
        { { "Draw", "绘制", "描画", "그리기", "Dibujar" } },
        { { "Curve", "曲线", "カーブ", "커브", "Curva" } },
        { { "Slice", "切割", "スライス", "자르기", "Cortar" } },
        { { "Trill {0}", "Trill {0}", "トリル {0}", "트릴 {0}", "Trino {0}" } },
        { { "Fade in {0} beats", "渐入 {0} 拍", "フェードイン {0} 拍", "페이드 인 {0}박", "Entrada gradual {0} tiempos" } },

        // ---- Curve 工具的右键菜单
        { { "Glide into legato notes (G)", "滑音连成连奏音符（G）", "グライドでレガートにつなぐ（G）", "글라이드로 레가토 연결 (G)", "Deslizar en notas ligadas (G)" } },
        { { "Vibrato and trill", "颤音与 Trill", "ビブラートとトリル", "비브라토와 트릴", "Vibrato y trino" } },
        { { "Vibrato on this segment", "此段加颤音", "この区間にビブラート", "이 구간에 비브라토", "Vibrato en este tramo" } },
        { { "Vibrato on this segment (add points first)", "此段加颤音（请先添加节点）", "この区間にビブラート（先にポイントを追加）", "이 구간에 비브라토 (먼저 포인트 추가)", "Vibrato en este tramo (añade puntos primero)" } },
        { { "Vibrato on whole note", "整个音符加颤音", "ノート全体にビブラート", "노트 전체에 비브라토", "Vibrato en toda la nota" } },
        { { "Trill on this segment", "此段加 Trill", "この区間にトリル", "이 구간에 트릴", "Trino en este tramo" } },
        { { "Trill on this segment (add points first)", "此段加 Trill（请先添加节点）", "この区間にトリル（先にポイントを追加）", "이 구간에 트릴 (먼저 포인트 추가)", "Trino en este tramo (añade puntos primero)" } },
        { { "Trill on whole note", "整个音符加 Trill", "ノート全体にトリル", "노트 전체에 트릴", "Trino en toda la nota" } },
        { { "Lock speed to tempo", "速度跟随曲速", "速さをテンポに同期", "속도를 템포에 동기화", "Sincronizar velocidad con el tempo" } },
        { { "Remove trill", "去掉 Trill", "トリルを削除", "트릴 제거", "Quitar trino" } },
        { { "Remove vibrato", "去掉颤音", "ビブラートを削除", "비브라토 제거", "Quitar vibrato" } },
        { { "Bend", "弯音", "ベンド", "벤드", "Bend" } },
        { { "Scoop (slide in from below)", "下方滑入（Scoop）", "スクープ（下からしゃくり上げ）", "스쿱 (아래에서 끌어올리기)", "Scoop (entrada desde abajo)" } },
        { { "Overshoot (go past, settle back)", "越过回落（Overshoot）", "オーバーシュート（行き過ぎて戻る）", "오버슈트 (지나쳤다 돌아오기)", "Overshoot (pasarse y volver)" } },
        { { "Bend and release", "推弦再放回", "ベンド＆リリース", "벤드 앤 릴리스", "Bend y retorno" } },
        { { "Fall (drop at the end)", "尾音下滑（Fall）", "フォール（終わりで下がる）", "폴 (끝에서 떨어짐)", "Fall (caída al final)" } },
        { { "Doit (flick up at the end)", "尾音上挑（Doit）", "ドゥイット（終わりで跳ね上がる）", "두잇 (끝에서 튕겨 올림)", "Doit (subida al final)" } },
        { { "No presets yet", "还没有预设", "プリセットはまだありません", "아직 프리셋이 없습니다", "Aún no hay presets" } },
        { { "Save curve as preset...", "将曲线存为预设...", "カーブをプリセットとして保存...", "커브를 프리셋으로 저장...", "Guardar curva como preset..." } },
        { { "Save curve as preset (draw a curve first)", "将曲线存为预设（请先画曲线）", "カーブをプリセットとして保存（先にカーブを描いてください）", "커브를 프리셋으로 저장 (먼저 커브를 그리세요)", "Guardar curva como preset (dibuja una curva primero)" } },
        { { "Delete preset", "删除预设", "プリセットを削除", "프리셋 삭제", "Eliminar preset" } },
        { { "Presets", "预设", "プリセット", "프리셋", "Presets" } },
        { { "Clear curve", "清除曲线", "カーブを消去", "커브 지우기", "Borrar curva" } },
        { { "Deleted preset \"{0}\"", "已删除预设“{0}”", "プリセット「{0}」を削除しました", "프리셋 \"{0}\" 삭제됨", "Preset \"{0}\" eliminado" } },

        // ---- 全部操作说明
        { { "Mouse & keyboard", "鼠标与键盘", "マウスとキーボード", "마우스와 키보드", "Ratón y teclado" } },
        { { "Click anywhere or press Esc to close", "单击任意处或按 Esc 关闭", "どこかをクリックするか Esc で閉じる", "아무 곳이나 클릭하거나 Esc를 눌러 닫기", "Haz clic en cualquier sitio o pulsa Esc para cerrar" } },
        { { "Notes", "音符", "ノート", "노트", "Notas" } },
        { { "Click empty space", "单击空白处", "空白をクリック", "빈 곳 클릭", "Clic en espacio vacío" } },
        { { "Add a note (Draw tool)", "添加音符（绘制工具）", "ノートを追加（描画ツール）", "노트 추가 (그리기 도구)", "Añadir nota (herramienta Dibujar)" } },
        { { "Drag after clicking", "单击后拖动", "クリック後にドラッグ", "클릭 후 드래그", "Arrastrar tras el clic" } },
        { { "Set the new note's length", "设置新音符的长度", "新しいノートの長さを設定", "새 노트 길이 설정", "Ajustar la duración de la nota nueva" } },
        { { "Drag a note", "拖动音符", "ノートをドラッグ", "노트 드래그", "Arrastrar una nota" } },
        { { "Move it (time and pitch)", "移动（时间和音高）", "移動（時間と音高）", "이동 (시간과 음높이)", "Moverla (tiempo y tono)" } },
        { { "Drag a note's right end", "拖动音符右端", "ノートの右端をドラッグ", "노트 오른쪽 끝 드래그", "Arrastrar el extremo derecho" } },
        { { "Change its length", "改变长度", "長さを変更", "길이 변경", "Cambiar su duración" } },
        { { "Alt + drag", "Alt + 拖动", "Alt + ドラッグ", "Alt + 드래그", "Alt + arrastrar" } },
        { { "Move or resize without snapping", "移动或改变长度时不吸附", "スナップせずに移動や長さを変更", "스냅 없이 이동 또는 길이 변경", "Mover o redimensionar sin ajuste" } },
        { { "Right-click / right-drag", "右键单击 / 右键拖动", "右クリック / 右ドラッグ", "우클릭 / 우클릭 드래그", "Clic derecho / arrastrar con derecho" } },
        { { "Delete notes", "删除音符", "ノートを削除", "노트 삭제", "Eliminar notas" } },
        { { "Drag a selected note's points", "拖动选中音符的节点", "選択したノートのポイントをドラッグ", "선택한 노트의 포인트 드래그", "Arrastrar puntos de una nota seleccionada" } },
        { { "Adjust its bend (no need for Curve)", "调整弯音（无需切到曲线工具）", "ベンドを調整（カーブツール不要）", "벤드 조정 (커브 도구 불필요)", "Ajustar su bend (sin la herramienta Curva)" } },
        { { "Drag in the VEL lane", "在 VEL 栏中拖动", "VEL レーンでドラッグ", "VEL 레인에서 드래그", "Arrastrar en el carril VEL" } },
        { { "Change velocity", "改变力度", "ベロシティを変更", "벨로시티 변경", "Cambiar velocidad" } },
        { { "Alt + wheel on a note", "在音符上 Alt + 滚轮", "ノート上で Alt + ホイール", "노트 위에서 Alt + 휠", "Alt + rueda sobre una nota" } },
        { { "Click a key", "单击琴键", "鍵盤をクリック", "건반 클릭", "Clic en una tecla" } },
        { { "Preview the pitch", "试听音高", "音高を試聴", "음높이 미리 듣기", "Escuchar el tono" } },
        { { "Selection", "选择", "選択", "선택", "Selección" } },
        { { "Ctrl + drag", "Ctrl + 拖动", "Ctrl + ドラッグ", "Ctrl + 드래그", "Ctrl + arrastrar" } },
        { { "Box select (any tool)", "框选（任何工具）", "範囲選択（どのツールでも）", "영역 선택 (모든 도구)", "Selección por recuadro (cualquier herramienta)" } },
        { { "Shift + click", "Shift + 单击", "Shift + クリック", "Shift + 클릭", "Shift + clic" } },
        { { "Add to / remove from selection", "加选 / 减选", "選択に追加 / 選択から外す", "선택에 추가 / 선택에서 제거", "Añadir a / quitar de la selección" } },
        { { "Ctrl + A", "Ctrl + A", "Ctrl + A", "Ctrl + A", "Ctrl + A" } },
        { { "Select all", "全选", "すべて選択", "모두 선택", "Seleccionar todo" } },
        { { "Esc", "Esc", "Esc", "Esc", "Esc" } },
        { { "Clear selection", "取消选择", "選択を解除", "선택 해제", "Quitar la selección" } },
        { { "Editing", "编辑", "編集", "편집", "Edición" } },
        { { "Delete", "Delete", "Delete", "Delete", "Supr" } },
        { { "Delete selected notes", "删除选中的音符", "選択したノートを削除", "선택한 노트 삭제", "Eliminar las notas seleccionadas" } },
        { { "Ctrl + C / X / V", "Ctrl + C / X / V", "Ctrl + C / X / V", "Ctrl + C / X / V", "Ctrl + C / X / V" } },
        { { "Copy / cut / paste at the mouse", "复制 / 剪切 / 粘贴到鼠标处", "コピー / カット / マウス位置に貼り付け", "복사 / 잘라내기 / 마우스 위치에 붙여넣기", "Copiar / cortar / pegar en el ratón" } },
        { { "Ctrl + B  or  Ctrl + D", "Ctrl + B 或 Ctrl + D", "Ctrl + B または Ctrl + D", "Ctrl + B 또는 Ctrl + D", "Ctrl + B  o  Ctrl + D" } },
        { { "Duplicate after the selection", "在选区后面复制一份", "選択範囲の後ろに複製", "선택 영역 뒤에 복제", "Duplicar tras la selección" } },
        { { "Transpose (Shift: octave)", "移调（Shift：八度）", "移調（Shift：オクターブ）", "조옮김 (Shift: 옥타브)", "Transponer (Shift: octava)" } },
        { { "Move by the snap step (Shift: bar)", "按吸附步长移动（Shift：一小节）", "スナップ単位で移動（Shift：1 小節）", "스냅 단위로 이동 (Shift: 마디)", "Mover por el paso de ajuste (Shift: compás)" } },
        { { "Ctrl + Z / Ctrl + Y", "Ctrl + Z / Ctrl + Y", "Ctrl + Z / Ctrl + Y", "Ctrl + Z / Ctrl + Y", "Ctrl + Z / Ctrl + Y" } },
        { { "Undo / redo", "撤销 / 重做", "元に戻す / やり直し", "실행 취소 / 다시 실행", "Deshacer / rehacer" } },
        { { "Pitch curves (Curve tool)", "音高曲线（曲线工具）", "ピッチカーブ（カーブツール）", "피치 커브 (커브 도구)", "Curvas de tono (herramienta Curva)" } },
        { { "Click a note", "单击音符", "ノートをクリック", "노트 클릭", "Clic en una nota" } },
        { { "Add a bend point (keep dragging)", "添加弯音节点（可继续拖动）", "ベンドポイントを追加（そのままドラッグ可）", "벤드 포인트 추가 (계속 드래그 가능)", "Añadir un punto de bend (sigue arrastrando)" } },
        { { "Drag a point", "拖动节点", "ポイントをドラッグ", "포인트 드래그", "Arrastrar un punto" } },
        { { "Move it (snaps to semitones, grid)", "移动（吸附到半音和网格）", "移動（半音とグリッドにスナップ）", "이동 (반음과 그리드에 스냅)", "Moverlo (se ajusta a semitonos y rejilla)" } },
        { { "Drag a small circle", "拖动小圆圈", "小さな円をドラッグ", "작은 원 드래그", "Arrastrar un círculo pequeño" } },
        { { "Change the curve shape", "改变曲线形状", "カーブの形を変更", "커브 모양 변경", "Cambiar la forma de la curva" } },
        { { "Right-click a point", "右键单击节点", "ポイントを右クリック", "포인트 우클릭", "Clic derecho en un punto" } },
        { { "Delete it", "删除节点", "削除", "삭제", "Eliminarlo" } },
        { { "Double-click a note", "双击音符", "ノートをダブルクリック", "노트 더블클릭", "Doble clic en una nota" } },
        { { "Remove its whole curve", "去掉整条曲线", "カーブ全体を削除", "커브 전체 제거", "Quitar toda su curva" } },
        { { "Right-click a note", "右键单击音符", "ノートを右クリック", "노트 우클릭", "Clic derecho en una nota" } },
        { { "Vibrato, trill, bends, presets", "颤音、Trill、弯音、预设", "ビブラート、トリル、ベンド、プリセット", "비브라토, 트릴, 벤드, 프리셋", "Vibrato, trino, bends, presets" } },
        { { "G  (2+ notes selected)", "G（选中 2 个以上音符）", "G（2 つ以上のノートを選択時）", "G (노트 2개 이상 선택)", "G  (2+ notas seleccionadas)" } },
        { { "Glide: merge a line or chords into legato notes", "滑音：把旋律或和弦连成连奏音符", "グライド：旋律や和音をレガートでつなぐ", "글라이드: 선율이나 화음을 레가토로 연결", "Deslizar: une una línea o acordes en notas ligadas" } },
        { { "Shift+click / Ctrl+drag", "Shift+单击 / Ctrl+拖动", "Shift+クリック / Ctrl+ドラッグ", "Shift+클릭 / Ctrl+드래그", "Shift+clic / Ctrl+arrastrar" } },
        { { "Select several notes", "选择多个音符", "複数のノートを選択", "여러 노트 선택", "Seleccionar varias notas" } },
        { { "Drag the diamond", "拖动菱形", "ひし形をドラッグ", "마름모 드래그", "Arrastrar el rombo" } },
        { { "Depth or trill note (up/down), speed (left/right)", "深度或 Trill 音程（上下），速度（左右）", "深さまたはトリルの音（上下）、速さ（左右）", "깊이 또는 트릴 음 (위아래), 속도 (좌우)", "Profundidad o nota del trino (arriba/abajo), velocidad (izq./der.)" } },
        { { "Drag the bar below", "拖动下方的小竖条", "下のバーをドラッグ", "아래 막대 드래그", "Arrastrar la barra inferior" } },
        { { "Vibrato fade-in length", "颤音渐入长度", "ビブラートのフェードインの長さ", "비브라토 페이드 인 길이", "Duración de la entrada del vibrato" } },
        { { "View & tools", "视图与工具", "表示とツール", "보기 및 도구", "Vista y herramientas" } },
        { { "Wheel", "滚轮", "ホイール", "휠", "Rueda" } },
        { { "Scroll up / down", "上下滚动", "上下にスクロール", "위아래 스크롤", "Desplazar arriba / abajo" } },
        { { "Shift + wheel", "Shift + 滚轮", "Shift + ホイール", "Shift + 휠", "Shift + rueda" } },
        { { "Scroll left / right", "左右滚动", "左右にスクロール", "좌우 스크롤", "Desplazar izquierda / derecha" } },
        { { "Ctrl + wheel", "Ctrl + 滚轮", "Ctrl + ホイール", "Ctrl + 휠", "Ctrl + rueda" } },
        { { "Zoom horizontally", "水平缩放", "横方向にズーム", "가로 확대/축소", "Zoom horizontal" } },
        { { "Ctrl + Shift + wheel", "Ctrl + Shift + 滚轮", "Ctrl + Shift + ホイール", "Ctrl + Shift + 휠", "Ctrl + Shift + rueda" } },
        { { "Zoom vertically", "垂直缩放", "縦方向にズーム", "세로 확대/축소", "Zoom vertical" } },
        { { "P  /  C  /  S", "P  /  C  /  S", "P  /  C  /  S", "P  /  C  /  S", "P  /  C  /  S" } },
        { { "Draw / Curve / Slice tool", "绘制 / 曲线 / 切割工具", "描画 / カーブ / スライスツール", "그리기 / 커브 / 자르기 도구", "Herramienta Dibujar / Curva / Cortar" } },
        { { "Slice: click / drag", "切割：单击 / 拖动", "スライス：クリック / ドラッグ", "자르기: 클릭 / 드래그", "Cortar: clic / arrastrar" } },
        { { "Cut a note / cut every note the line crosses", "切开一个音符 / 切开竖线经过的所有音符", "ノートを切る / 線が通るすべてのノートを切る", "노트 자르기 / 선이 지나는 모든 노트 자르기", "Cortar una nota / cortar todas las que cruza la línea" } },
        { { "Drop a MIDI file", "拖入 MIDI 文件", "MIDI ファイルをドロップ", "MIDI 파일 놓기", "Soltar un archivo MIDI" } },
        { { "Add its notes (pitch bends become curves)", "添加其中的音符（弯音变成曲线）", "ノートを追加（ピッチベンドはカーブに）", "노트 추가 (피치 벤드는 커브로)", "Añadir sus notas (los pitch bends se vuelven curvas)" } },
        { { "Drag the MIDI button", "拖动 MIDI 按钮", "MIDI ボタンをドラッグ", "MIDI 버튼 드래그", "Arrastrar el botón MIDI" } },
        { { "Export to FL or another DAW", "导出到 FL 或其他 DAW", "FL や他の DAW に書き出し", "FL 또는 다른 DAW로 내보내기", "Exportar a FL u otro DAW" } },
        { { "F1  or  ?", "F1 或 ?", "F1 または ?", "F1 또는 ?", "F1  o  ?" } },
        { { "Show this list", "显示此列表", "この一覧を表示", "이 목록 표시", "Mostrar esta lista" } },

        // ---- 存为预设
        { { "Curve {0}", "曲线 {0}", "カーブ {0}", "커브 {0}", "Curva {0}" } },
        { { "Save curve as preset", "将曲线存为预设", "カーブをプリセットとして保存", "커브를 프리셋으로 저장", "Guardar curva como preset" } },
        { { "A preset with this name will be replaced", "将替换同名的预设", "同じ名前のプリセットは置き換えられます", "같은 이름의 프리셋이 대체됩니다", "Se reemplazará el preset con este nombre" } },
        { { "Saved for all projects (stretches to each note)", "所有工程通用（按音符长度伸缩）", "すべてのプロジェクトで使えます（ノートの長さに合わせて伸縮）", "모든 프로젝트에서 사용 (노트 길이에 맞춰 늘어남)", "Se guarda para todos los proyectos (se ajusta a cada nota)" } },
        { { "Replaced preset \"{0}\"", "已替换预设“{0}”", "プリセット「{0}」を置き換えました", "프리셋 \"{0}\" 대체됨", "Preset \"{0}\" reemplazado" } },
        { { "Saved preset \"{0}\"", "已保存预设“{0}”", "プリセット「{0}」を保存しました", "프리셋 \"{0}\" 저장됨", "Preset \"{0}\" guardado" } },
        { { "Could not save the preset ({0})", "无法保存预设（{0}）", "プリセットを保存できませんでした（{0}）", "프리셋을 저장할 수 없습니다 ({0})", "No se pudo guardar el preset ({0})" } },
        { { "Save", "保存", "保存", "저장", "Guardar" } },
        { { "Cancel", "取消", "キャンセル", "취소", "Cancelar" } },

        // ---- 卷帘的按钮和提示
        { { "All mouse and keyboard actions (F1)", "全部鼠标和键盘操作（F1）", "すべてのマウスとキーボードの操作（F1）", "모든 마우스 및 키보드 조작 (F1)", "Todas las acciones de ratón y teclado (F1)" } },
        { { "Draw (P): click to add notes, drag to set their length", "绘制（P）：单击添加音符，拖动设置长度", "描画（P）：クリックでノートを追加、ドラッグで長さを設定", "그리기 (P): 클릭으로 노트 추가, 드래그로 길이 설정", "Dibujar (P): clic para añadir notas, arrastra para fijar su duración" } },
        { { "Curve (C): click a note to add bend points, drag them to bend the pitch", "曲线（C）：单击音符添加弯音节点，拖动节点弯曲音高", "カーブ（C）：ノートをクリックしてベンドポイントを追加、ドラッグで音高を曲げる", "커브 (C): 노트를 클릭해 벤드 포인트 추가, 드래그해 음높이를 휘게 함", "Curva (C): clic en una nota para añadir puntos de bend, arrástralos para curvar el tono" } },
        { { "Slice (S): click a note to cut it, drag up or down to cut several", "切割（S）：单击音符切开，上下拖动切开多个", "スライス（S）：ノートをクリックで切断、上下にドラッグで複数を切断", "자르기 (S): 노트를 클릭해 자르고, 위아래로 드래그해 여러 개 자르기", "Cortar (S): clic en una nota para cortarla, arrastra arriba o abajo para cortar varias" } },
        { { "Pitch bend range. Set your synth's bend range to the same value.", "弯音范围。请把音源的弯音范围设成相同的值。", "ピッチベンドの範囲。音源のベンド範囲も同じ値に設定してください。", "피치 벤드 범위. 신스의 벤드 범위도 같은 값으로 설정하세요.", "Rango de pitch bend. Ajusta el rango de bend del sintetizador al mismo valor." } },
        { { "Drag into FL Studio (or any DAW) to export the notes with their bends. Click for more options.", "拖到 FL Studio（或其他 DAW）里，导出带弯音的音符。单击查看更多选项。", "FL Studio（または他の DAW）にドラッグすると、ベンド付きでノートを書き出します。クリックでその他のオプション。", "FL Studio(또는 다른 DAW)로 드래그하면 벤드가 포함된 노트를 내보냅니다. 클릭하면 추가 옵션이 나옵니다.", "Arrastra a FL Studio (u otro DAW) para exportar las notas con sus bends. Haz clic para más opciones." } },
        { { "Drop to add the notes at bar {0}", "松开以在第 {0} 小节添加音符", "ドロップすると {0} 小節目にノートを追加", "놓으면 {0}마디에 노트 추가", "Suelta para añadir las notas en el compás {0}" } },
        { { "Click note  Add point    Right-click note  Vibrato, trill, presets    Right-click point  Delete    Alt  No snap",
            "单击音符  添加节点    右键音符  颤音、Trill、预设    右键节点  删除    Alt  不吸附",
            "ノートをクリック  ポイント追加    ノートを右クリック  ビブラート、トリル、プリセット    ポイントを右クリック  削除    Alt  スナップなし",
            "노트 클릭  포인트 추가    노트 우클릭  비브라토, 트릴, 프리셋    포인트 우클릭  삭제    Alt  스냅 없음",
            "Clic en nota  Añadir punto    Clic derecho en nota  Vibrato, trino, presets    Clic derecho en punto  Eliminar    Alt  Sin ajuste" } },
        { { "Click a note  Cut it    Drag up or down  Cut several    Alt  No snap",
            "单击音符  切开    上下拖动  切开多个    Alt  不吸附",
            "ノートをクリック  切断    上下にドラッグ  複数を切断    Alt  スナップなし",
            "노트 클릭  자르기    위아래 드래그  여러 개 자르기    Alt  스냅 없음",
            "Clic en nota  Cortar    Arrastrar arriba o abajo  Cortar varias    Alt  Sin ajuste" } },
        { { "Drag points  Bend    Shift+click  Add or remove    \xe2\x86\x91\xe2\x86\x93  Transpose    Del  Delete",
            "拖动节点  弯音    Shift+单击  加选或减选    \xe2\x86\x91\xe2\x86\x93  移调    Del  删除",
            "ポイントをドラッグ  ベンド    Shift+クリック  選択の追加や解除    \xe2\x86\x91\xe2\x86\x93  移調    Del  削除",
            "포인트 드래그  벤드    Shift+클릭  추가 또는 제거    \xe2\x86\x91\xe2\x86\x93  조옮김    Del  삭제",
            "Arrastrar puntos  Bend    Shift+clic  Añadir o quitar    \xe2\x86\x91\xe2\x86\x93  Transponer    Supr  Eliminar" } },
        { { "Shift+click  Add or remove    \xe2\x86\x91\xe2\x86\x93  Transpose    Ctrl+B  Duplicate    Del  Delete",
            "Shift+单击  加选或减选    \xe2\x86\x91\xe2\x86\x93  移调    Ctrl+B  复制一份    Del  删除",
            "Shift+クリック  選択の追加や解除    \xe2\x86\x91\xe2\x86\x93  移調    Ctrl+B  複製    Del  削除",
            "Shift+클릭  추가 또는 제거    \xe2\x86\x91\xe2\x86\x93  조옮김    Ctrl+B  복제    Del  삭제",
            "Shift+clic  Añadir o quitar    \xe2\x86\x91\xe2\x86\x93  Transponer    Ctrl+B  Duplicar    Supr  Eliminar" } },
        { { "Ctrl+drag  Box select    Shift+click  Select several    Right-click  Delete    Alt+drag  No snap",
            "Ctrl+拖动  框选    Shift+单击  多选    右键  删除    Alt+拖动  不吸附",
            "Ctrl+ドラッグ  範囲選択    Shift+クリック  複数選択    右クリック  削除    Alt+ドラッグ  スナップなし",
            "Ctrl+드래그  영역 선택    Shift+클릭  여러 개 선택    우클릭  삭제    Alt+드래그  스냅 없음",
            "Ctrl+arrastrar  Selección    Shift+clic  Selección múltiple    Clic derecho  Eliminar    Alt+arrastrar  Sin ajuste" } },
        { { "{0} notes (red outline) have to share synth copies: too many different bends at once (limit {1}). While they overlap, they follow the newest one's bend.",
            "{0} 个音符（红框）只能共用音源副本：同时弯向不同的音符太多（上限 {1}）。重叠期间，它们会跟随最新那个音符的弯音。",
            "{0} 個のノート（赤枠）は音源のコピーを共有しています。異なるベンドが同時に多すぎます（上限 {1}）。重なっている間は、最も新しいノートのベンドに従います。",
            "{0}개 노트(빨간 테두리)가 신스 복사본을 공유합니다. 동시에 서로 다른 벤드가 너무 많습니다 (한도 {1}). 겹치는 동안에는 가장 최근 노트의 벤드를 따릅니다.",
            "{0} notas (contorno rojo) deben compartir copias del sintetizador: demasiados bends distintos a la vez (límite {1}). Mientras se solapan, siguen el bend de la más reciente." } },
        { { "Some bends go past the bend range. Raise Bend (and your synth's range to match).",
            "有些弯音超出了弯音范围。请调大底栏的弯音范围（音源的范围也要一致）。",
            "一部のベンドがベンド範囲を超えています。ベンド範囲を広げてください（音源の範囲も合わせてください）。",
            "일부 벤드가 벤드 범위를 넘습니다. 벤드 범위를 늘리세요 (신스의 범위도 맞추세요).",
            "Algunos bends superan el rango. Aumenta Bend (y el rango del sintetizador para que coincida)." } },
        { { "Pitch bend range (semitones)", "弯音范围（半音）", "ピッチベンドの範囲（半音）", "피치 벤드 범위 (반음)", "Rango de pitch bend (semitonos)" } },
        { { "Set your synth to the same value", "音源也要设成相同的值", "音源も同じ値に設定してください", "신스도 같은 값으로 설정하세요", "Ajusta el sintetizador al mismo valor" } },
        { { "Select two or more notes or chords to glide between (Shift+click or Ctrl+drag).",
            "请选择两个以上的音符或和弦来滑音（Shift+单击或 Ctrl+拖动）。",
            "グライドするには 2 つ以上のノートか和音を選択してください（Shift+クリックまたは Ctrl+ドラッグ）。",
            "글라이드하려면 노트나 화음을 2개 이상 선택하세요 (Shift+클릭 또는 Ctrl+드래그).",
            "Selecciona dos o más notas o acordes para deslizar entre ellos (Shift+clic o Ctrl+arrastrar)." } },
        { { "All selected notes start together. Select notes or chords at two or more different times.",
            "选中的音符都在同一时刻开始。请选择两个以上不同时刻的音符或和弦。",
            "選択したノートはすべて同時に始まります。2 つ以上の異なるタイミングのノートか和音を選択してください。",
            "선택한 노트가 모두 같은 시점에 시작합니다. 서로 다른 시점의 노트나 화음을 2개 이상 선택하세요.",
            "Todas las notas seleccionadas empiezan a la vez. Selecciona notas o acordes en dos o más momentos distintos." } },
        { { "Can't glide chords with more than 12 notes.", "无法对超过 12 个音的和弦滑音。", "12 音を超える和音はグライドできません。", "12음이 넘는 화음은 글라이드할 수 없습니다.", "No se pueden deslizar acordes de más de 12 notas." } },
        { { "This glide needs Bend \xc2\xb1{0} or more. Raise Bend (and your synth's range to match).",
            "这个滑音需要 \xc2\xb1{0} 以上的弯音范围。请调大弯音范围（音源的范围也要一致）。",
            "このグライドには \xc2\xb1{0} 以上のベンド範囲が必要です。ベンド範囲を広げてください（音源の範囲も合わせてください）。",
            "이 글라이드에는 \xc2\xb1{0} 이상의 벤드 범위가 필요합니다. 벤드 범위를 늘리세요 (신스의 범위도 맞추세요).",
            "Este deslizamiento necesita Bend \xc2\xb1{0} o más. Aumenta Bend (y el rango del sintetizador para que coincida)." } },

        // ---- MIDI
        { { "Added {0} notes from {1}", "已从 {1} 添加 {0} 个音符", "{1} から {0} 個のノートを追加しました", "{1}에서 노트 {0}개를 추가했습니다", "Se añadieron {0} notas de {1}" } },
        { { "Added {0} notes from {1} ({2} with pitch bends)", "已从 {1} 添加 {0} 个音符（其中 {2} 个带弯音）", "{1} から {0} 個のノートを追加しました（{2} 個はピッチベンド付き）", "{1}에서 노트 {0}개를 추가했습니다 ({2}개는 피치 벤드 포함)", "Se añadieron {0} notas de {1} ({2} con pitch bend)" } },
        { { "Add notes from a MIDI file", "从 MIDI 文件添加音符", "MIDI ファイルからノートを追加", "MIDI 파일에서 노트 추가", "Añadir notas de un archivo MIDI" } },
        { { "{0} notes had to share a channel with a different bend", "{0} 个音符只能和弯音不同的音符共用通道", "{0} 個のノートは異なるベンドのノートとチャンネルを共有しました", "{0}개 노트가 다른 벤드의 노트와 채널을 공유했습니다", "{0} notas tuvieron que compartir canal con un bend distinto" } },
        { { "{0} notes bend further than \xc2\xb1" "48", "{0} 个音符的弯音超过 \xc2\xb1" "48", "{0} 個のノートのベンドが \xc2\xb1" "48 を超えています", "{0}개 노트의 벤드가 \xc2\xb1" "48을 넘습니다", "{0} notas superan \xc2\xb1" "48 de bend" } },
        { { "{0} notes bend further than the bend range", "{0} 个音符的弯音超出弯音范围", "{0} 個のノートのベンドが範囲を超えています", "{0}개 노트의 벤드가 범위를 넘습니다", "{0} notas superan el rango de bend" } },
        { { ", ", "，", "、", ", ", ", " } },
        { { "There are no notes to export yet.", "还没有可以导出的音符。", "書き出すノートがまだありません。", "아직 내보낼 노트가 없습니다.", "Aún no hay notas para exportar." } },
        { { "Could not write the MIDI file ({0})", "无法写入 MIDI 文件（{0}）", "MIDI ファイルを書き込めませんでした（{0}）", "MIDI 파일을 쓸 수 없습니다 ({0})", "No se pudo escribir el archivo MIDI ({0})" } },
        { { "Exported, but {0}.", "已导出，但{0}。", "書き出しましたが、{0}。", "내보냈지만 {0}.", "Exportado, pero {0}." } },
        { { "Save MIDI file", "保存 MIDI 文件", "MIDI ファイルを保存", "MIDI 파일 저장", "Guardar archivo MIDI" } },
        { { "Could not write {0}", "无法写入 {0}", "{0} を書き込めませんでした", "{0}을(를) 쓸 수 없습니다", "No se pudo escribir {0}" } },
        { { "Saved {0}", "已保存 {0}", "{0} を保存しました", "{0} 저장됨", "Guardado {0}" } },
        { { "Saved {0} ({1})", "已保存 {0}（{1}）", "{0} を保存しました（{1}）", "{0} 저장됨 ({1})", "Guardado {0} ({1})" } },
        { { "Drag this button into your DAW to export", "把这个按钮拖到 DAW 里即可导出", "このボタンを DAW にドラッグして書き出し", "이 버튼을 DAW로 드래그해 내보내기", "Arrastra este botón a tu DAW para exportar" } },
        { { "Drag exports one channel per voice", "拖出：每个声部一个通道", "ドラッグの書き出し：ボイスごとに 1 チャンネル", "드래그 내보내기: 보이스마다 채널 1개", "Arrastrar exporta un canal por voz" } },
        { { "Drag exports MPE", "拖出：MPE", "ドラッグの書き出し：MPE", "드래그 내보내기: MPE", "Arrastrar exporta MPE" } },
        { { "Save MIDI file (one channel per voice)...", "保存 MIDI 文件（每个声部一个通道）...", "MIDI ファイルを保存（ボイスごとに 1 チャンネル）...", "MIDI 파일 저장 (보이스마다 채널 1개)...", "Guardar archivo MIDI (un canal por voz)..." } },
        { { "Save MPE MIDI file...", "保存 MPE MIDI 文件...", "MPE MIDI ファイルを保存...", "MPE MIDI 파일 저장...", "Guardar archivo MIDI MPE..." } },
        { { "Add notes from a MIDI file...", "从 MIDI 文件添加音符...", "MIDI ファイルからノートを追加...", "MIDI 파일에서 노트 추가...", "Añadir notas de un archivo MIDI..." } },

        // ---- 顶栏、音源
        { { "Piano roll", "钢琴卷帘", "ピアノロール", "피아노 롤", "Piano roll" } },
        { { "Synth", "音源", "音源", "신스", "Sinte" } },
        { { "Synth (!)", "音源（!）", "音源（!）", "신스 (!)", "Sinte (!)" } },
        { { "Play Curvable's notes along the host's timeline", "跟随宿主时间线播放 Curvable 的音符", "ホストのタイムラインに合わせて Curvable のノートを再生", "호스트 타임라인에 맞춰 Curvable 노트 재생", "Reproduce las notas de Curvable con la línea de tiempo del host" } },
        { { "A note in the host's piano roll starts playback; it loops while the note is held. Host notes make no sound.",
            "宿主钢琴卷帘里的音符触发播放，按住期间循环。宿主的音符本身不发声。",
            "ホストのピアノロールのノートで再生が始まり、押している間ループします。ホストのノート自体は鳴りません。",
            "호스트 피아노 롤의 노트가 재생을 시작하고, 누르고 있는 동안 반복됩니다. 호스트 노트 자체는 소리가 나지 않습니다.",
            "Una nota en el piano roll del host inicia la reproducción; se repite mientras se mantiene. Las notas del host no suenan." } },
        { { "Don't play Curvable's notes", "不播放 Curvable 的音符", "Curvable のノートを再生しない", "Curvable 노트를 재생하지 않음", "No reproducir las notas de Curvable" } },
        { { "For synths with MPE turned on: one copy of the synth, every bending note on its own MIDI channel. Bend in the piano roll must match the synth's MPE bend range (usually 48), which is separate from the patch's normal bend range.",
            "用于已打开 MPE 的音源：只用一个音源实例，每个弯音的音符占一个 MIDI 通道。卷帘里的弯音范围必须与音源的 MPE 弯音范围一致（通常是 48），它和音色里的普通弯音范围是分开的。",
            "MPE を有効にした音源用：音源は 1 つだけ使い、ベンドするノートごとに MIDI チャンネルを割り当てます。ピアノロールのベンド範囲は音源の MPE ベンド範囲（通常 48）に合わせてください。これはパッチの通常のベンド範囲とは別の設定です。",
            "MPE를 켠 신스용: 신스를 하나만 쓰고, 벤드하는 노트마다 MIDI 채널을 따로 씁니다. 피아노 롤의 벤드 범위는 신스의 MPE 벤드 범위(보통 48)와 같아야 하며, 이는 패치의 일반 벤드 범위와 별개입니다.",
            "Para sintetizadores con MPE activado: una sola copia del sintetizador y cada nota con bend en su propio canal MIDI. Bend en el piano roll debe coincidir con el rango de bend MPE del sintetizador (normalmente 48), que es distinto del rango de bend normal del patch." } },
        { { "MPE on. Turn on MPE in the synth. Its MPE bend range (usually 48, not the patch's normal bend range) must match Bend {0}.",
            "MPE 已打开。请在音源里也打开 MPE，并把它的 MPE 弯音范围（通常是 48，不是音色里的普通弯音范围）设成 {0}，与卷帘一致。",
            "MPE オン。音源でも MPE を有効にしてください。音源の MPE ベンド範囲（通常 48。パッチの通常のベンド範囲ではありません）を {0} に合わせてください。",
            "MPE 켜짐. 신스에서도 MPE를 켜세요. 신스의 MPE 벤드 범위(보통 48, 패치의 일반 벤드 범위가 아님)가 {0}과 같아야 합니다.",
            "MPE activado. Activa MPE en el sintetizador. Su rango de bend MPE (normalmente 48, no el rango normal del patch) debe coincidir con Bend {0}." } },
        { { "Settings", "设置", "設定", "설정", "Ajustes" } },
        { { "Drop to load this synth", "松开以加载此音源", "ドロップしてこの音源を読み込む", "놓으면 이 신스를 불러옵니다", "Suelta para cargar este sintetizador" } },
        { { "Curvable now runs several copies of your synth so notes can bend on their own. Turn off the synth's own reverb and delay and use FL's effects instead, or they will stack up.",
            "Curvable 现在运行了多个音源副本，让每个音符能单独弯音。请关掉音源自带的混响和延迟，改用 FL 的效果器，否则效果会叠加。",
            "Curvable はノートを個別にベンドできるよう、音源のコピーを複数動かしています。音源内蔵のリバーブとディレイはオフにして FL のエフェクトを使ってください。そうしないと効果が重なります。",
            "Curvable은 노트마다 따로 벤드할 수 있도록 신스 복사본을 여러 개 실행합니다. 신스 자체의 리버브와 딜레이는 끄고 FL의 이펙트를 쓰세요. 그렇지 않으면 효과가 겹칩니다.",
            "Curvable ejecuta varias copias de tu sintetizador para que cada nota tenga su propio bend. Desactiva la reverb y el delay del sintetizador y usa los efectos de FL, o se acumularán." } },
        { { "Got it", "知道了", "了解", "확인", "Entendido" } },
        { { "Automation slots: let FL automate knobs of the synth", "自动化槽：让 FL 自动化音源的旋钮", "オートメーションスロット：FL から音源のノブをオートメーション", "오토메이션 슬롯: FL에서 신스 노브를 자동화", "Ranuras de automatización: deja que FL automatice perillas del sintetizador" } },
        { { "AUTOMATION", "自动化", "オートメーション", "오토메이션", "AUTOMATIZACIÓN" } },
        { { "Loading...", "正在加载...", "読み込み中...", "불러오는 중...", "Cargando..." } },
        { { "{0}  \xc3\x97{1} ({2} on)", "{0}  \xc3\x97{1}（{2} 个运行中）", "{0}  \xc3\x97{1}（{2} 個稼働中）", "{0}  \xc3\x97{1} ({2}개 실행 중)", "{0}  \xc3\x97{1} ({2} activas)" } },
        { { "MPE mode: one copy of the synth, a MIDI channel per bending note.", "MPE 模式：只用一个音源实例，每个弯音的音符一个 MIDI 通道。", "MPE モード：音源は 1 つ、ベンドするノートごとに MIDI チャンネル 1 つ。", "MPE 모드: 신스 하나, 벤드하는 노트마다 MIDI 채널 하나.", "Modo MPE: una copia del sintetizador, un canal MIDI por nota con bend." } },
        { { "{0} copies of the synth let notes bend independently. Idle copies pause to save CPU (\"on\" = currently running).",
            "{0} 个音源副本让音符能独立弯音。空闲的副本会暂停以节省 CPU（“运行中”= 正在运算）。",
            "音源のコピー {0} 個でノートを個別にベンドできます。アイドル中のコピーは CPU 節約のため停止します（「稼働中」= 現在動作中）。",
            "신스 복사본 {0}개로 노트를 독립적으로 벤드합니다. 쉬는 복사본은 CPU 절약을 위해 일시 정지합니다 (\"실행 중\" = 현재 동작 중).",
            "{0} copias del sintetizador permiten bends independientes. Las copias inactivas se pausan para ahorrar CPU (\"activas\" = en marcha)." } },
        { { "Missing: {0}", "缺失：{0}", "見つかりません：{0}", "없음: {0}", "Falta: {0}" } },
        { { "Load synth", "加载音源", "音源を読み込む", "신스 불러오기", "Cargar sinte" } },
        { { "Could not create another copy of the synth ({0}). Some bent notes play unbent for now; retrying shortly.",
            "无法再创建音源副本（{0}）。部分弯音的音符暂时不会弯音，稍后会重试。",
            "音源のコピーをこれ以上作成できませんでした（{0}）。一部のノートはしばらくベンドなしで鳴ります。まもなく再試行します。",
            "신스 복사본을 더 만들 수 없습니다 ({0}). 일부 노트는 당분간 벤드 없이 재생되며, 곧 다시 시도합니다.",
            "No se pudo crear otra copia del sintetizador ({0}). Algunas notas suenan sin bend por ahora; se reintentará en breve." } },
        { { "Loading synth...", "正在加载音源...", "音源を読み込み中...", "신스 불러오는 중...", "Cargando sintetizador..." } },
        { { "No synth loaded. Click \"Load synth\" or drag a plugin here.",
            "还没有加载音源。单击“加载音源”，或把插件拖到这里。",
            "音源が読み込まれていません。「音源を読み込む」をクリックするか、プラグインをここにドラッグしてください。",
            "불러온 신스가 없습니다. \"신스 불러오기\"를 클릭하거나 플러그인을 여기로 드래그하세요.",
            "No hay sintetizador cargado. Haz clic en \"Cargar sinte\" o arrastra un plugin aquí." } },
        { { "Theme", "主题", "テーマ", "테마", "Tema" } },
        { { "Day", "浅色", "ライト", "라이트", "Claro" } },
        { { "Night", "深色", "ダーク", "다크", "Oscuro" } },
        { { "Language", "语言", "言語", "언어", "Idioma" } },
        { { "Clear all notes", "清除所有音符", "すべてのノートを消去", "모든 노트 지우기", "Borrar todas las notas" } },
        { { "Sync synth copies now", "立即同步音源副本", "音源のコピーを今すぐ同期", "신스 복사본 지금 동기화", "Sincronizar copias ahora" } },
        { { "Plugin manager...", "插件管理器...", "プラグインマネージャー...", "플러그인 관리자...", "Gestor de plugins..." } },
        { { "Mouse & keyboard shortcuts (F1)", "鼠标与键盘快捷键（F1）", "マウスとキーボードのショートカット（F1）", "마우스 및 키보드 단축키 (F1)", "Atajos de ratón y teclado (F1)" } },
        { { "Curvable - Plugin manager", "Curvable - 插件管理器", "Curvable - プラグインマネージャー", "Curvable - 플러그인 관리자", "Curvable - Gestor de plugins" } },
        { { "Choose a synth plugin", "选择音源插件", "音源プラグインを選択", "신스 플러그인 선택", "Elegir un plugin de sintetizador" } },
        { { "Reading {0}...", "正在读取 {0}...", "{0} を読み込み中...", "{0} 읽는 중...", "Leyendo {0}..." } },
        { { "Could not load a synth from {0}. It may not be an instrument, or it failed to start.",
            "无法从 {0} 加载音源。它可能不是乐器插件，或者启动失败。",
            "{0} から音源を読み込めませんでした。インストゥルメントではないか、起動に失敗した可能性があります。",
            "{0}에서 신스를 불러올 수 없습니다. 악기가 아니거나 시작에 실패했을 수 있습니다.",
            "No se pudo cargar un sintetizador de {0}. Puede que no sea un instrumento o que no haya podido iniciarse." } },

        // ---- 自动化槽
        { { "Click, then move a knob in the synth to link it to this slot", "单击后转动音源里的一个旋钮，把它关联到这个槽", "クリックしてから音源のノブを動かすと、このスロットにリンクします", "클릭한 뒤 신스의 노브를 움직이면 이 슬롯에 연결됩니다", "Haz clic y mueve una perilla del sintetizador para vincularla a esta ranura" } },
        { { "Unlink", "取消关联", "リンク解除", "연결 해제", "Desvincular" } },
        { { "Move a knob", "请转动旋钮", "ノブを動かす", "노브를 움직이세요", "Mueve una perilla" } },
        { { "Learn", "学习", "ラーン", "학습", "Aprender" } },
        { { "Empty", "空", "空き", "비어 있음", "Vacía" } },
        { { "Automation slots", "自动化槽", "オートメーションスロット", "오토메이션 슬롯", "Ranuras de automatización" } },
        { { "Automate these in FL. Learn links a slot to a synth knob; all copies follow.",
            "在 FL 里自动化这些槽。“学习”可把槽关联到音源的旋钮，所有副本都会跟随。",
            "これらを FL でオートメーションします。ラーンでスロットを音源のノブにリンクでき、すべてのコピーが追従します。",
            "FL에서 이 슬롯들을 자동화하세요. 학습으로 슬롯을 신스 노브에 연결하면 모든 복사본이 따라갑니다.",
            "Automatízalas en FL. Aprender vincula una ranura a una perilla del sintetizador; todas las copias la siguen." } },
        { { "Load a synth to link its knobs to these slots.", "加载音源后，才能把它的旋钮关联到这些槽。", "音源を読み込むと、ノブをこれらのスロットにリンクできます。", "신스를 불러오면 노브를 이 슬롯에 연결할 수 있습니다.", "Carga un sintetizador para vincular sus perillas a estas ranuras." } },
        { { "Close", "关闭", "閉じる", "닫기", "Cerrar" } },

        // ---- 音源浏览器
        { { "Type to search synths...", "输入以搜索音源...", "音源を検索...", "신스 검색...", "Escribe para buscar sintetizadores..." } },
        { { "No synths found.\nDrag a .vst3 file here, browse for one, or scan in the plugin manager.",
            "没有找到音源。\n把 .vst3 文件拖到这里、浏览选择，或在插件管理器里扫描。",
            "音源が見つかりません。\n.vst3 ファイルをここにドラッグするか、ファイルを選ぶか、プラグインマネージャーでスキャンしてください。",
            "신스를 찾을 수 없습니다.\n.vst3 파일을 여기로 드래그하거나, 찾아보거나, 플러그인 관리자에서 스캔하세요.",
            "No se encontraron sintetizadores.\nArrastra aquí un archivo .vst3, búscalo o escanea en el gestor de plugins." } },
        { { "No synth matches \"{0}\".", "没有与“{0}”匹配的音源。", "「{0}」に一致する音源はありません。", "\"{0}\"와(과) 일치하는 신스가 없습니다.", "Ningún sintetizador coincide con \"{0}\"." } },
        { { "loaded", "已加载", "読み込み済み", "불러옴", "cargado" } },
        { { "recent", "最近", "最近", "최근", "reciente" } },
        { { "Browse for file...", "浏览文件...", "ファイルを選択...", "파일 찾아보기...", "Buscar archivo..." } },
        { { "Unload synth", "卸载音源", "音源を外す", "신스 내리기", "Quitar sintetizador" } },
    };

    const std::unordered_map<std::string, std::size_t>& index()
    {
        static const auto map = []
        {
            std::unordered_map<std::string, std::size_t> m;

            for (std::size_t i = 0; i < table.size(); ++i)
                m.emplace (table[i].text[0], i);

            return m;
        }();

        return map;
    }

    constexpr std::array<LanguageInfo, numLanguages> languageInfo { {
        { Language::english,  "en", "English" },
        { Language::chinese,  "zh", "简体中文" },
        { Language::japanese, "ja", "日本語" },
        { Language::korean,   "ko", "한국어" },
        { Language::spanish,  "es", "Español" },
    } };
}

const std::array<LanguageInfo, numLanguages>& getLanguages() { return languageInfo; }

Language languageFromCode (const juce::String& code)
{
    for (const auto& info : languageInfo)
        if (code == info.code)
            return info.language;

    return Language::english;
}

const char* codeOf (Language language)
{
    return languageInfo[(std::size_t) language].code;
}

void setLanguage (Language language)
{
    if (current.exchange (language) != language)
        listeners().call ([] (Listener& l) { l.languageChanged(); });
}

Language getLanguage() noexcept { return current.load(); }

juce::String tr (const char* english, Language language)
{
    if (language != Language::english)
        if (const auto it = index().find (english); it != index().end())
            if (const auto* text = table[it->second].text[(std::size_t) language]; text != nullptr && *text != 0)
                return juce::String::fromUTF8 (text);

    return juce::String::fromUTF8 (english);
}

juce::String tr (const char* english) { return tr (english, getLanguage()); }

const std::vector<Entry>& getEntries() { return table; }

bool hasEntry (const juce::String& english)
{
    return index().count (english.toStdString()) != 0;
}

void addListener (Listener* l)    { listeners().add (l); }
void removeListener (Listener* l) { listeners().remove (l); }

} // namespace curvable::i18n
