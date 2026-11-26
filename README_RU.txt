PanVK WSI Test — сборка APK через GitHub Actions
================================================

Тебе не нужен ноутбук и не нужно собирать приложение локально. GitHub Actions соберёт APK на сервере GitHub.

1. Скачай ZIP этого проекта и распакуй его.
2. Создай новый пустой репозиторий на github.com (например, PanVK-WSI-Test).
3. Загрузи в репозиторий ВСЁ содержимое распакованной папки, включая скрытую папку .github.
   Важно: файл workflow должен оказаться по пути .github/workflows/build.yml в корне репозитория.
4. Открой вкладку Actions. Если GitHub спросит разрешение на workflows, нажми I understand my workflows, go ahead and enable them.
5. Выбери workflow "Build PanVK WSI Test APK", нажми Run workflow → Run workflow.
   Он также запускается автоматически после push в ветку main.
6. Дождись зелёной галочки. Открой завершённый запуск workflow, прокрути вниз до Artifacts и скачай PanVK-WSI-Test-debug.
7. Распакуй скачанный artifact ZIP. Внутри будет app-debug.apk. Установи его на телефон.

Если GitHub не показывает Actions:
- Проверь, что файл .github/workflows/build.yml действительно загружен в корень репозитория.
- Вкладка Actions → выбери workflow слева.
- Запуск вручную доступен через кнопку Run workflow.

После установки:
1. В XHYN V выбери PanVK.
2. Полностью закрой XHYN V.
3. Запусти PanVK WSI Test.
4. Пришли текст результата на экране. Для дополнительного лога:
   adb logcat -c
   запусти приложение, затем:
   adb logcat -d -s PanVK_WSI_TEST

Что делает тест:
Проверяет Vulkan instance, Android Surface, graphics/present queue, surface capabilities,
создание swapchain, acquire, submit и present. DXVK и GTA V не используются.

Примечание:
APK будет debug-подписи GitHub Actions. Для диагностического приложения этого достаточно.
