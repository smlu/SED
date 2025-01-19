unit RenderUtils;
interface
uses Geometry, J_Level, System.Types;

type
  TFrustumPlane = record
    distance: Double;
    position: TVector; // TODO: update code to not rely on this position field
    normal: TVector;
  end;

  TClipFrustum = class
    nearPlane: TFrustumPlane;
    farPlane: TFrustumPlane;
    leftPlane: TFrustumPlane;
    topPlane: TFrustumPlane;
    rightPlane: TFrustumPlane;
    bottomPlane: TFrustumPlane;

    // Function checks if sphere is in frustum.
    // center should be in camera space
    // Returns:
    //   0 - inside frustum
    //   1 - intersects frustum
    //   2 - outside frustum
    function IsSphereInFrustum(center: TVector; radius: Double): Integer; // center should be transformed to camera space
  end;

  TCamera = class
    private
      frustum_: TClipFrustum;
      fov_: Double;
      aspect_: Double;
      focalLength_: Double;
      sector_: TJKSector;
      procedure BuildFrustum;

    public
      position: TVector;
      rotation: TVector;
      canvasWidth,
      canvasHeight: Double;

      orient: TMatrix43; // camera transformation matrix (updated via call to Update method)
      view: TMatrix43; // view matrix (updated via call to Update method)

      constructor CreatePerspective(width, height: double;  fov: Double; nearPlane, farPlane: Double);
      destructor Destroy; override;

      procedure Update; // Updates camera's view, orient matrices and frustum
      procedure Reset;

      property aspect: Double read aspect_;
      property fov: Double read fov_;
      property focalLength: Double read focalLength_;
      property frustum: TClipFrustum read frustum_;
      property sector: TJKSector read sector_ ; // current sector camera is in. Can be null

      procedure MoveBackward(step: double);
      procedure MoveForward(step: double);
      procedure MoveLeft(step: double);
      procedure MoveRight(step: double);
      procedure MoveUp(step: double);
      procedure MoveDown(step: double);
      procedure Translate(dx, dy, dz: double); // translates camera from current position in direction of current camera rotation for x,y,z steps
      procedure MoveTo(pos, pyr: TVector); // moves camera to pos position and orient it to pyr

      procedure Rotate(pitch, yaw, roll: Double); overload;  // Rotates current camera orientation camera for pitch, yaw roll angles
      procedure Rotate(pyr: TVector); overload;
      procedure RotatePitch(pitch: Double);
      procedure RotateYaw(yaw: Double);
      procedure RotateRoll(roll: Double);
  end;

implementation
uses GlobalVars, lev_utils, System.Math, U_tbar;

constructor TCamera.CreatePerspective(width, height: double;  fov: Double; nearPlane, farPlane: Double);
begin
  self.fov_     := fov;
  self.aspect_  := width / height;
  self.position := TVector.zero;
  self.rotation := TVector.zero;
  canvasWidth   := width;
  canvasHeight  := height;

  var hheight := height * 0.5;
  var hwidth  := width * 0.5;

  // TODO: make HOR+
  fov := DegToRad(fov);

  focalLength_ := hwidth / Tan(fov * 0.5);
  //self.fov_ := fov;

  frustum_ := TClipFrustum.Create;
  frustum_.nearPlane.distance   :=  nearPlane;
  frustum_.farPlane.distance    :=  farPlane;
  frustum_.topPlane.distance    :=  hheight / focalLength_ / aspect_;
  frustum_.bottomPlane.distance := -hheight / focalLength_ / aspect_;
  frustum_.leftPlane.distance   := -hwidth  / focalLength_;
  frustum_.rightPlane.distance  :=  hwidth  / focalLength_;
  BuildFrustum;
end;

destructor TCamera.Destroy;
begin
  frustum_.Free;
end;


procedure TCamera.BuildFrustum;
begin
  frustum_.leftPlane.normal
    .SetCoords(-focalLength_, 0.0, -(canvasWidth * 0.5));
  frustum_.leftPlane.normal.Normalize;

  frustum_.rightPlane.normal
    .SetCoords(-frustum_.leftPlane.normal.x, frustum_.leftPlane.normal.y, frustum_.leftPlane.normal.z);

  frustum_.topPlane.normal
    .SetCoords(0.0, -(canvasHeight * 0.5), focalLength_);
  frustum_.topPlane.normal.Normalize;

  frustum_.bottomPlane.normal
    .SetCoords(frustum_.topPlane.normal.x, frustum_.topPlane.normal.y, -frustum_.topPlane.normal.z);
end;

procedure TCamera.Update;
  function IsSectorVisible(const sec: TJKSector): Boolean;
  begin
    Result := (not P3DVisLayers) or ToolBar.IsLayerVisible(sec.layer);
  end;

begin
  orient := TMatrix43.Create(position, rotation);
  view   := orient.InvertOrtho; // inverse orient to get view matrix

  // Update cur sector
  with position do
    begin
      try
        if CheckSectorRef(sector_) and IsSectorVisible(sector_) then
          if IsInSector(sector_, x, y, z) then
            exit;
      except // skip exception in case  sector_ was deleted
      end;

      sector_ := nil;
      var sc := FindSectorForXYZ(level, x, y, z, (*visibleOnly*) P3DVisLayers);
      if sc <> -1 then
        begin
          var sec := level.sectors[sc];
          if IsSectorVisible(sec) then
            sector_ := sec;
        end;
    end;
end;

procedure TCamera.Reset;
begin
  sector_ := nil;
  position := TVector.zero;
  rotation := TVector.zero;
end;

procedure TCamera.Translate(dx, dy, dz: double);
  var d: TVector;
begin
  d.dx := dx;
  d.dy := dy;
  d.dz := dz;

  with rotation do
    RotateVector(d, pitch, yaw, 0);

  with position do
    begin
      x := x - d.dx;
      y := y + d.dy;
      z := z - d.dz;
    end;
end;

procedure TCamera.MoveTo(pos, pyr: TVector);
begin
  position := pos;
  rotation := pyr;
end;

procedure TCamera.MoveForward(step: double);
begin
  Translate(0, step, 0);
end;

procedure TCamera.MoveBackward(step: double);
begin
  Translate(0, -step, 0);
end;

procedure TCamera.MoveLeft(step: double);
begin
  Translate(step, 0, 0);
end;

procedure TCamera.MoveRight(step: double);
begin
  Translate(-step, 0, 0);
end;

procedure TCamera.MoveUp(step: double);
begin
  with position do
    z := z + step;
end;

procedure TCamera.MoveDown(step: double);
begin
  with position do
    z := z - step;
end;

procedure TCamera.Rotate(pitch, yaw, roll: Double);
begin
  RotatePitch(pitch);
  RotateYaw(yaw);
  RotateRoll(roll);
end;

procedure TCamera.Rotate(pyr: TVector);
begin
  with pyr do
    Rotate(pitch, yaw, roll);
end;

procedure TCamera.RotatePitch(pitch: Double);
begin
  rotation.pitch := rotation.pitch + pitch;
  if rotation.pitch  < -90 then
    rotation.pitch := -90
  else if  rotation.pitch > 90 then
    rotation.pitch := 90;
end;

procedure TCamera.RotateYaw(yaw: Double);
begin
  rotation.yaw := rotation.yaw + yaw;
  if rotation.yaw  < 0 then
    rotation.yaw := rotation.yaw + 360
  else if  rotation.yaw  > 360 then
    rotation.yaw := rotation.yaw - 360;
end;

procedure TCamera.RotateRoll(roll: Double);
begin
  rotation.roll := rotation.roll + roll;
  if rotation.roll  < -90 then
    rotation.roll := -90
  else if  rotation.roll > 90 then
    rotation.roll := 90;
end;

function TClipFrustum.IsSphereInFrustum(center: TVector; radius: Double): Integer;
var
  res: Integer;
  d,  negRadius: Double;
begin
  res := 1;
  var frontBound := radius + center.y;
  var backBound := center.y - radius;

  if frontBound < nearPlane.distance then
  begin
    Result := 2;
    Exit;
  end;

  if backBound < nearPlane.distance then
    res := 0;

//  if bClipFar then
//  begin
//    if backBound > farPlane.distance then
//    begin
//      Result := 2;
//      Exit;
//    end;
//
//    if frontBound > farPlane.distance then
//      res := 0;
//  end;

  d := topPlane.normal.z * center.z + topPlane.normal.y * center.y;
  if d > radius then
  begin
    Result := 2;
    Exit;
  end;

  negRadius := -radius;
  if d > negRadius then
    res := 0;

  d := bottomPlane.normal.y * center.y + bottomPlane.normal.z * center.z;
  if d > radius then
  begin
    Result := 2;
    Exit;
  end;

  if d > negRadius then
    res := 0;

  d := leftPlane.normal.y * center.y + leftPlane.normal.x * center.x;
  if d > radius then
  begin
    Result := 2;
    Exit;
  end;

  if d > negRadius then
    res := 0;

  d := rightPlane.normal.y * center.y + rightPlane.normal.x * center.x;
  if d > radius then
  begin
    Result := 2;
    Exit;
  end;

  if d > negRadius then
    res := 0;

  Result := IfThen(res = 1, 0, 1);
end;

end.
