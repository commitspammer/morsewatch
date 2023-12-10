{
  description = "MorseWatch";
  
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
  let
    pkgs = import nixpkgs { system = "x86_64-linux"; };
  in {
    packages."x86_64-linux" = {
      default = pkgs.stdenv.mkDerivation {
        name = "morsewatch";
        # src = ./.;
        nativeBuildInputs = with pkgs; [
          go
          httpie
        ];
        # buildInputs = with pkgs; [
        # ];
      };
    };
  };

  nixConfig = {
    bash-prompt-prefix = "dev:";
  };

}
