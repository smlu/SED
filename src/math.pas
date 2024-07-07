unit math;

interface
// vector functions
procedure VectorAdd2(const x1, y1: double; const x2, y2: double; var rx, ry: double); inline;
function VectorCross2(const x1, y1: double; const x2, y2: double): double; inline;
function VectorDot2(const x1, y1: double; const x2, y2: double): double; inline;
function VectorDist2(const x1, y1: double; const x2, y2: double): double; inline;
function VectorDistSqr2(const x1, y1: double; const x2, y2: double): double; inline;
function VectorLen2(const x, y: double): double; inline;
function VectorLenSqr2(const x, y: double): double; inline;
function VectorNormalize2(const x, y: double; var nx, ny: double): double; overload;  inline;
function VectorNormalize2(var x, y: double): double; overload;  inline;
procedure VectorScale2(const x, y: double; const scalar: double; var sx, sy: double); inline;
procedure VectorSub2(const x1, y1: double; const x2, y2: double; var rx, ry: double); inline;

procedure VectorAdd3(const x1, y1, z1: double; const x2, y2, z2: double; var rx, ry, rz: double); inline;
procedure VectorCross3(const x1, y1, z1: double; const x2, y2, z2: double; var cx, cy, cz: double);  inline;
function VectorDist3(const x1, y1, z1: double; const x2, y2, z2: double): double; inline;
function VectorDistSqr3(const x1, y1, z1: double; const x2, y2, z2: double): double; inline;
function VectorDot3(const x1, y1, z1: double; const x2, y2, z2: double): double; inline;
function VectorLen3(const x, y, z: double): double; inline;
function VectorLenSqr3(const x, y, z: double): double; inline;
function VectorNormalize3(const x, y, z: double; var nx, ny, nz: double): double; overload; inline;  // Returns vec len
function VectorNormalize3(var x, y, z: double): double; overload; inline;
procedure VectorProject3(const ax, ay, az: double; const bx, by, bz: double; var px, py, pz: double); inline; // Projects vector a onto vector b resulting in vector p
procedure VectorProjectOnToNormal3(const ax, ay, az: double; const nx, ny, nz: double; var px, py, pz: double); inline; // Projects vector a onto normal unit vector n resulting in vector p
procedure VectorScale3(const x, y, z: double; const scalar: double; var sx, sy, sz: double); inline;
procedure VectorSub3(const x1, y1, z1: double; const x2, y2, z2: double; var rx, ry, rz: double); inline;

implementation
procedure VectorAdd2(const x1, y1: double; const x2, y2: double; var rx, ry: double);
begin
  rx := x1 + x2;
  ry := y1 + y2;
end;

function VectorCross2(const x1, y1: double; const x2, y2: double): double;
begin
   Result := x1 * y2 - x2 * y2;
end;

function VectorDot2(const x1, y1: double; const x2, y2: double): double;
begin
  Result := x1 * x2 + y1 + y2;
end;

function VectorDist2(const x1, y1: double; const x2, y2: double): double;
begin
  Result := sqrt(VectorDistSqr2(x1, y1, x2, y2));
end;

function VectorDistSqr2(const x1, y1: double; const x2, y2: double): double;
begin
  Result := sqr(x1 - x2) + sqr(y1 - y2);
end;

function VectorLen2(const x, y: double): double;
begin
  Result := sqrt(VectorLenSqr2(x, y));
end;

function VectorLenSqr2(const x, y: double): double; inline;
begin
  Result := x * x + y * y;
end;

function VectorNormalize2(const x, y: double; var nx, ny: double): double; overload;
begin
  Result := VectorLen2(x, y);
  if Result = 0 then
    exit;
  nx := x / Result;
  ny := y / Result;
end;

function VectorNormalize2(var x, y: double): double; overload;
begin
  Result := VectorNormalize2(x, y, x, y);
end;

procedure VectorScale2(const x, y: double; const scalar: double; var sx, sy: double);
begin
  sx := x * scalar;
  sy := y * scalar;
end;

procedure VectorSub2(const x1, y1: double; const x2, y2: double; var rx, ry: double);
begin
  rx := x1 - x2;
  ry := y1 - y2;
end;

procedure VectorAdd3(const x1, y1, z1: double; const x2, y2, z2: double; var rx, ry, rz: double);
begin
  rx := x1 + x2;
  ry := y1 + y2;
  rz := z1 + z2;
end;

procedure VectorCross3(const x1, y1, z1: double; const x2, y2, z2: double; var cx, cy, cz: double);
begin
   cx := y1 * z2 - z1 * y2;
   cy := z1 * x2 - x1 * z2;
   cz := x1 * y2 - x2 * y1;
end;

function VectorDist3(const x1, y1, z1: double; const x2, y2, z2: double): double;
begin
  Result := sqrt(VectorDistSqr3(x1, y1, z1, x2, y2, z2));
end;

function VectorDistSqr3(const x1, y1, z1: double; const x2, y2, z2: double): double;
begin
  Result := sqr(x1 - x2) + sqr(y1 - y2) + sqr(z1 - z2);
end;

function VectorDot3(const x1, y1, z1: double; const x2, y2, z2: double): double;
begin
  Result := x1 * x2 + y1 * y2 + z1 * z2;
end;

function VectorLen3(const x, y, z: double): double;
begin
  Result := sqrt(VectorLenSqr3(x, y, z));
end;

function VectorLenSqr3(const x, y, z: double): double; inline;
begin
  Result := x * x + y * y + z * z;
end;

function VectorNormalize3(const x, y, z: double; var nx, ny, nz: double): double;
begin
  Result := VectorLen3(x, y, z);
  if Result = 0 then
    exit;
  nx := x / Result;
  ny := y / Result;
  nz := z / Result;
end;

function VectorNormalize3(var x, y, z: double): double;
begin
  Result := VectorNormalize3(x, y, z, x, y, z);
end;

procedure VectorProject3(const ax, ay, az: double; const bx, by, bz: double; var px, py, pz: double);
begin
  // Project vector a onto vector b, resulting in vector p
  //
  //       /|
  //      / |
  //   a /  |
  //    /   |
  //   /    |
  //  /     |
  // x------>--------->
  //     p        b
  var len := VectorDot3(ax, ay, az, bx, by, bz) / VectorDot3(bx, by, bz, bx, by, bz);
  VectorScale3(bx, by, bz, len, px, py, pz);
end;

procedure VectorProjectOnToNormal3(const ax, ay, az: double; const nx, ny, nz: double; var px, py, pz: double);
begin
  // Project vector a onto unit normal vector n, resulting in vector p
  //
  //       /|
  //      / |
  //   a /  |
  //    /   |
  //   /    |
  //  /     |
  // x->---->
  //  n  p
  var len := VectorDot3(ax, ay, az, nx, ny, nz);
  VectorScale3(nx, ny, nz, len, px, py, pz);
end;

procedure VectorScale3(const x, y, z: double; const scalar: double; var sx, sy, sz: double);
begin
  sx := x * scalar;
  sy := y * scalar;
  sz := z * scalar;
end;

procedure VectorSub3(const x1, y1, z1: double; const x2, y2, z2: double; var rx, ry, rz: double);
begin
  rx := x1 - x2;
  ry := y1 - y2;
  rz := z1 - z2;
end;

end.
