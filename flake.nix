{
  description = "A development environment for a gtkmm4 C++ project";

  inputs = {
    # 1. Пиннинг nixpkgs для воспроизводимости
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable"; # Используйте стабильную ветку
  };

  outputs = { nixpkgs, ... }:
  let
    # Определение целевой системы (архитектуры)
    system = "x86_64-linux";
    pkgs = import nixpkgs { inherit system; };
  in
  {
    # 2. Определение среды разработки (devShell)
    devShells.${system}.default = pkgs.mkShell {

      # nativeBuildInputs: Инструменты, которые запускаются во время сборки (cmake, make, pkg-config).
      # Они попадают в PATH.
      nativeBuildInputs = with pkgs; [
        cmake        # Настройка проекта
        gnumake      # Сборка
        pkg-config   # Поиск библиотек
      ];

      # buildInputs: Библиотеки, с которыми будет линковаться ваша программа (gtkmm4, libsigcxx4).
      # Nix автоматически добавляет их пути в PKG_CONFIG_PATH и CMAKE_PREFIX_PATH.
      buildInputs = with pkgs; [
        gtkmm4
        libsigcxx # Важная зависимость для gtkmm
      ];

      # Дополнительные шаги, если нужно что-то специфичное
      shellHook = ''
        echo "C++ среда для gtkmm4 загружена. Используйте 'cmake -B build' и 'make -C build'."
      '';
    };
  };
}