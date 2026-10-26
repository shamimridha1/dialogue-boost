// Dialogue Boost editor: macOS Cocoa UI (same layout as Windows)
#include "plugin.h"
#import <Cocoa/Cocoa.h>
#include <cmath>

using namespace Steinberg;

namespace dlb {
static const CGFloat W = 460, H = 300;
static const CGFloat TX0 = 24, TX1 = 436;
static const CGFloat ROW_Y[3] = {92, 152, 212};
static const Steinberg::Vst::ParamID ROW_ID[3] = {kBoost, kClarity, kCeiling};
static NSString* const ROW_LABEL[3] = {@"BOOST", @"CLARITY", @"CEILING"};
static NSString* const ROW_HINT[3] = {@"output volume", @"voice EQ amount", @"max peak level"};
static const double ROW_DEFAULT[3] = {0.5, 0.5, 0.75};
static const NSRect BYPASS_RC = {{356, 22}, {80, 26}};

static NSColor* rgb (int r, int g, int b)
{
	return [NSColor colorWithSRGBRed:r / 255.0 green:g / 255.0 blue:b / 255.0 alpha:1.0];
}
} // namespace dlb

using namespace dlb;

@interface DLB6D1BView : NSView
{
@public
	dlb::Editor* editor;
	int dragRow;
	NSTimer* timer;
}
@end

@implementation DLB6D1BView

- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent*)e { return YES; }

- (void)fill:(NSColor*)c rect:(NSRect)r radius:(CGFloat)rad
{
	[c setFill];
	[[NSBezierPath bezierPathWithRoundedRect:r xRadius:rad yRadius:rad] fill];
}

- (void)text:(NSString*)s at:(NSPoint)p font:(NSFont*)f color:(NSColor*)c
{
	[s drawAtPoint:p withAttributes:@{NSFontAttributeName : f, NSForegroundColorAttributeName : c}];
}

- (void)textRight:(NSString*)s rightX:(CGFloat)x y:(CGFloat)y font:(NSFont*)f color:(NSColor*)c
{
	NSDictionary* a = @{NSFontAttributeName : f, NSForegroundColorAttributeName : c};
	NSSize sz = [s sizeWithAttributes:a];
	[s drawAtPoint:NSMakePoint (x - sz.width, y) withAttributes:a];
}

- (void)drawRect:(NSRect)dirty
{
	if (!editor) return;
	PluginEntry* p = editor->plug;
	NSColor *bg = rgb (23, 24, 28), *panel = rgb (31, 32, 38), *track = rgb (46, 48, 56),
	        *accent = rgb (232, 163, 61), *txt = rgb (236, 236, 238), *dim = rgb (138, 141, 150),
	        *off = rgb (90, 92, 100);
	[bg setFill];
	NSRectFill (self.bounds);

	NSFont* fTitle = [NSFont systemFontOfSize:16 weight:NSFontWeightBold];
	NSFont* fSub = [NSFont systemFontOfSize:11.5];
	NSFont* fLabel = [NSFont systemFontOfSize:10.5 weight:NSFontWeightBold];
	NSFont* fValue = [NSFont systemFontOfSize:13 weight:NSFontWeightBold];

	bool bypassed = p->getParamNormalized (kBypass) > 0.5;

	[self text:@"DIALOGUE BOOST" at:NSMakePoint (23, 18) font:fTitle color:txt];
	[self text:@"Louder, clearer voice — no clipping" at:NSMakePoint (24, 42) font:fSub color:dim];

	[self fill:(bypassed ? track : accent) rect:BYPASS_RC radius:13];
	{
		NSString* s = bypassed ? @"BYPASSED" : @"ACTIVE";
		NSDictionary* a = @{NSFontAttributeName : fLabel,
		                    NSForegroundColorAttributeName : (bypassed ? dim : bg)};
		NSSize sz = [s sizeWithAttributes:a];
		[s drawAtPoint:NSMakePoint (NSMidX (BYPASS_RC) - sz.width / 2, NSMidY (BYPASS_RC) - sz.height / 2)
		    withAttributes:a];
	}

	[panel setFill];
	NSRectFill (NSMakeRect (0, 72, W, 1));

	for (int i = 0; i < 3; ++i)
	{
		CGFloat y = ROW_Y[i];
		double norm = p->getParamNormalized (ROW_ID[i]);
		double pv = p->plain (ROW_ID[i]);
		NSString* val = i == 0 ? [NSString stringWithFormat:@"+%.1f dB", pv]
		              : i == 1 ? [NSString stringWithFormat:@"%.0f %%", pv]
		                       : [NSString stringWithFormat:@"%.1f dB", pv];
		NSDictionary* la = @{NSFontAttributeName : fLabel};
		CGFloat lw = [ROW_LABEL[i] sizeWithAttributes:la].width;
		[self text:ROW_LABEL[i] at:NSMakePoint (TX0, y) font:fLabel color:txt];
		[self text:ROW_HINT[i] at:NSMakePoint (TX0 + lw + 6, y - 1) font:fSub color:dim];
		[self textRight:val rightX:TX1 y:y - 3 font:fValue color:(bypassed ? off : txt)];

		CGFloat ty = y + 28, th = 6;
		[self fill:track rect:NSMakeRect (TX0, ty, TX1 - TX0, th) radius:3];
		CGFloat fx = TX0 + norm * (TX1 - TX0);
		if (fx - TX0 > 1) [self fill:(bypassed ? off : accent) rect:NSMakeRect (TX0, ty, fx - TX0, th) radius:3];
		[(bypassed ? dim : txt) setFill];
		[[NSBezierPath bezierPathWithOvalInRect:NSMakeRect (fx - 8, ty + th / 2 - 8, 16, 16)] fill];
	}

	[panel setFill];
	NSRectFill (NSMakeRect (0, 258, W, 1));
	float gr = editor->shownGr;
	[self text:@"LIMITING" at:NSMakePoint (TX0, 270) font:fLabel color:dim];
	CGFloat mx = 100, mw = 250;
	[self fill:track rect:NSMakeRect (mx, 276, mw, 4) radius:2];
	CGFloat gw = std::min (1.0f, gr / 12.f) * mw;
	if (gw > 1) [self fill:(gr > 6.f ? rgb (224, 92, 72) : accent) rect:NSMakeRect (mx, 276, gw, 4) radius:2];
	[self textRight:(gr < 0.05f ? @"0.0 dB" : [NSString stringWithFormat:@"-%.1f dB", gr])
	         rightX:TX1 y:269 font:fSub color:dim];
}

- (int)rowAt:(NSPoint)pt
{
	if (pt.x < TX0 - 14 || pt.x > TX1 + 14) return -1;
	for (int i = 0; i < 3; ++i)
		if (pt.y >= ROW_Y[i] - 4 && pt.y <= ROW_Y[i] + 46) return i;
	return -1;
}

static double xToNorm (CGFloat x)
{
	double n = (x - TX0) / (TX1 - TX0);
	return n < 0 ? 0 : (n > 1 ? 1 : n);
}

- (void)mouseDown:(NSEvent*)e
{
	if (!editor) return;
	NSPoint pt = [self convertPoint:e.locationInWindow fromView:nil];
	if (NSPointInRect (pt, BYPASS_RC))
	{
		double b = editor->plug->getParamNormalized (kBypass) > 0.5 ? 0.0 : 1.0;
		editor->plug->editParam (kBypass, b, 0);
		editor->plug->editParam (kBypass, b, 2);
		[self setNeedsDisplay:YES];
		return;
	}
	int row = [self rowAt:pt];
	if (row < 0) return;
	if (e.clickCount == 2)
	{
		editor->plug->editParam (ROW_ID[row], ROW_DEFAULT[row], 0);
		editor->plug->editParam (ROW_ID[row], ROW_DEFAULT[row], 2);
		dragRow = -1;
	}
	else
	{
		dragRow = row;
		editor->plug->editParam (ROW_ID[row], xToNorm (pt.x), 0);
	}
	[self setNeedsDisplay:YES];
}

- (void)mouseDragged:(NSEvent*)e
{
	if (!editor || dragRow < 0) return;
	NSPoint pt = [self convertPoint:e.locationInWindow fromView:nil];
	editor->plug->editParam (ROW_ID[dragRow], xToNorm (pt.x), 1);
	[self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent*)e
{
	if (!editor || dragRow < 0) return;
	int row = dragRow;
	dragRow = -1;
	editor->plug->editParam (ROW_ID[row], editor->plug->getParamNormalized (ROW_ID[row]), 2);
	[self setNeedsDisplay:YES];
}

- (void)scrollWheel:(NSEvent*)e
{
	if (!editor) return;
	NSPoint pt = [self convertPoint:e.locationInWindow fromView:nil];
	int row = [self rowAt:pt];
	if (row < 0 || e.scrollingDeltaY == 0) return;
	double step = (e.scrollingDeltaY > 0 ? 1.0 : -1.0) / 48.0;
	double v = editor->plug->getParamNormalized (ROW_ID[row]) + step;
	editor->plug->editParam (ROW_ID[row], v, 0);
	editor->plug->editParam (ROW_ID[row], v, 2);
	[self setNeedsDisplay:YES];
}

- (void)tick:(NSTimer*)t
{
	if (!editor) return;
	float target = editor->plug->gainReduction ();
	float s = editor->shownGr;
	s = target > s ? target : s + (target - s) * 0.25f;
	if (std::fabs (s - editor->shownGr) > 0.01f)
	{
		editor->shownGr = s;
		[self setNeedsDisplayInRect:NSMakeRect (0, 260, W, H - 260)];
	}
}
@end

namespace dlb {

Editor::Editor (PluginEntry* owner) : plug (owner)
{
	ViewRect r (0, 0, (int32)W, (int32)H);
	setRect (r);
}

Editor::~Editor ()
{
	if (plug && plug->editor == this) plug->editor = nullptr;
}

tresult PLUGIN_API Editor::isPlatformTypeSupported (FIDString type)
{
	return (type && strcmp (type, kPlatformTypeNSView) == 0) ? kResultTrue : kResultFalse;
}

void Editor::attachedToParent ()
{
	DLB6D1BView* v = [[DLB6D1BView alloc] initWithFrame:NSMakeRect (0, 0, W, H)];
	v->editor = this;
	v->dragRow = -1;
	[(NSView*)systemWindow addSubview:v];
	v->timer = [NSTimer scheduledTimerWithTimeInterval:0.04
	                                            target:v
	                                          selector:@selector (tick:)
	                                          userInfo:nil
	                                           repeats:YES];
	hwnd = (void*)v;
	plug->editor = this;
}

void Editor::removedFromParent ()
{
	plug->editor = nullptr;
	if (hwnd)
	{
		DLB6D1BView* v = (DLB6D1BView*)hwnd;
		[v->timer invalidate];
		v->timer = nil;
		v->editor = nullptr;
		[v removeFromSuperview];
		[v release];
		hwnd = nullptr;
	}
}

void Editor::refresh ()
{
	if (!hwnd) return;
	DLB6D1BView* v = (DLB6D1BView*)hwnd;
	if ([NSThread isMainThread]) [v setNeedsDisplay:YES];
	else
	{
		[v retain];
		dispatch_async (dispatch_get_main_queue (), ^{
			if (v->editor) [v setNeedsDisplay:YES];
			[v release];
		});
	}
}

} // namespace dlb
