// make_icon.swift -- the app icon, drawn: a deep blue field, "263" in heavy white, a sound wave under it; 1024 x
// 1024, opaque (the App Store refuses an icon with alpha).  xcrun swift tools/make_icon.swift app/Assets.xcassets/AppIcon.appiconset/icon-1024.png
// then tools/make_icon.sh for the Mac sizes.  MIT.
import AppKit
import ImageIO
let size = 1024
// an opaque sRGB bitmap (noneSkipLast: no alpha in the icon, as the App Store requires)
let ctx = CGContext(data: nil, width: size, height: size, bitsPerComponent: 8, bytesPerRow: 0,
                    space: CGColorSpace(name: CGColorSpace.sRGB)!, bitmapInfo: CGImageAlphaInfo.noneSkipLast.rawValue)!
NSGraphicsContext.saveGraphicsState()
NSGraphicsContext.current = NSGraphicsContext(cgContext: ctx, flipped: false)
let s = CGFloat(size)
let colors = [CGColor(srgbRed: 0.12, green: 0.25, blue: 0.58, alpha: 1),
              CGColor(srgbRed: 0.03, green: 0.07, blue: 0.22, alpha: 1)] as CFArray
let grad = CGGradient(colorsSpace: CGColorSpace(name: CGColorSpace.sRGB), colors: colors, locations: [0, 1])!
ctx.drawLinearGradient(grad, start: CGPoint(x: 0, y: s), end: CGPoint(x: s, y: 0), options: [])
let style = NSMutableParagraphStyle(); style.alignment = .center
let attrs: [NSAttributedString.Key: Any] = [.font: NSFont.systemFont(ofSize: 360, weight: .black),
                                            .foregroundColor: NSColor.white, .paragraphStyle: style, .kern: -10]
NSAttributedString(string: "263", attributes: attrs).draw(in: NSRect(x: 0, y: 420, width: s, height: 430))
ctx.setFillColor(CGColor(srgbRed: 0.45, green: 0.86, blue: 1.0, alpha: 1))
let bars = 21, barW: CGFloat = 22, gap: CGFloat = 14
let total = CGFloat(bars) * barW + CGFloat(bars - 1) * gap
for i in 0..<bars {
    let t = Double(i) / Double(bars - 1) * 2 - 1
    let h = CGFloat(30 + 190 * abs(sin(Double(i) * 0.9)) * exp(-2.2 * t * t))
    let x = (s - total) / 2 + CGFloat(i) * (barW + gap)
    ctx.addPath(CGPath(roundedRect: CGRect(x: x, y: 280 - h / 2, width: barW, height: h), cornerWidth: 11,
                       cornerHeight: 11, transform: nil))
}
ctx.fillPath()
NSGraphicsContext.restoreGraphicsState()
let url = URL(fileURLWithPath: CommandLine.arguments[1]) as CFURL
let dest = CGImageDestinationCreateWithURL(url, "public.png" as CFString, 1, nil)!
CGImageDestinationAddImage(dest, ctx.makeImage()!, nil)
precondition(CGImageDestinationFinalize(dest))
