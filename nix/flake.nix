{
 inputs.nixpkgs.url = "github:NixOS/nixpkgs/f13ff45afd1bb73e640eaa08a7066dbed07e3238";
 outputs = { self, nixpkgs, ... }: let p = import nixpkgs { system = "x86_64-linux"; config.allowUnfree = true; }; in {
 devShells.x86_64-linux = {
  default = p.mkShell { packages = [ p.open-watcom-v2 p.python3 p.dosbox-x ]; };
  ci = p.mkShell { packages = [ p.python3 p.dosbox-x ]; };
 };
 };
}
