unit GLContext;

interface
uses Windows, dglOpenGL, SysUtils;

type

  TGLContext = class
  private
    refCounter: Cardinal;
    whdc: HDC;
    hrc: HGLRC;
  public
    constructor Create(hdc: HDC; option: TRCOptions = [opDoubleBuffered]; colotBits: Integer = 8; ZBufferBits: Integer = 32; stencilBits: Integer = 0);
    destructor Destroy; override;
    procedure Acquire; // Acquire gl contex. Note can throw exception
    procedure Release; // Release gl contex
  end;

implementation

constructor TGLContext.Create(hdc: HDC; option: TRCOptions; colotBits: Integer; ZBufferBits: Integer; stencilBits: Integer);
begin
  inherited Create;

  refCounter := 0;
  whdc       := hdc;

  // Create a new OpenGL rendering context.
  // Note this will also default initialize OpenGL and throws exception if there is an error
  hrc := CreateRenderingContext(hdc, option, colotBits, ZBufferBits, stencilBits, 0, 0, 0);
  //hrc := CreateRenderingContextVersion(hdc, [opDoubleBuffered], (*maj*)3, (*min*)0, (*ForwardCompatible*)False, (*ColotBits*)8, (*ZBits*)32, (*StencilBits*)0, 0, 0, 0);  // ForwardCompatible:= True after moved away from immediate mode

  if hrc = 0 then
    RaiseLastOSError;

end;

destructor TGLContext.Destroy;
begin
  // If this context is still current, release it.
  if wglGetCurrentContext = hrc then
    DeactivateRenderingContext();
  // Delete the OpenGL rendering context.
  if hrc <> 0 then
    DestroyRenderingContext(hrc);
  inherited;
end;

procedure TGLContext.Acquire;
begin
  // If context is not acquired yet, make it current.
  if (refCounter = 0) (*or (wglGetCurrentContext <> hrc)*) then
  begin
    if not ActivateRenderingContext(whdc, hrc) then
      RaiseLastOSError;
  end;
  Inc(refCounter);
end;

procedure TGLContext.Release;
begin
  if refCounter > 0 then
  begin
    Dec(refCounter);
    if refCounter = 0 then
      DeactivateRenderingContext();
  end;
end;

end.
