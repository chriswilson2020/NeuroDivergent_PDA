import SwiftUI

enum DesktopPaneMetrics {
    static func masterWidth(_ available: CGFloat) -> CGFloat {min(340,max(260,available*0.30))}
    static func usesColumns(_ available: CGFloat) -> Bool {available >= 680}
}

enum DesktopFlowMetrics {
    static func arrange(sizes:[CGSize],width:CGFloat,spacing:CGFloat) -> (size:CGSize,points:[CGPoint]) {
        let width=max(1,width)
        var x:CGFloat=0,y:CGFloat=0,rowHeight:CGFloat=0,points:[CGPoint]=[]
        for size in sizes {
            if x > 0 && x+size.width > width {x=0;y += rowHeight+spacing;rowHeight=0}
            points.append(CGPoint(x:x,y:y));x += size.width+spacing;rowHeight=max(rowHeight,size.height)
        }
        return (CGSize(width:width,height:y+rowHeight),points)
    }
}

// Buttons keep their readable intrinsic sizes, moving to another row instead
// of being squeezed, overlapping, or forcing the window wider.
struct DesktopFlow: Layout {
    var spacing: CGFloat=10
    func sizeThatFits(proposal: ProposedViewSize,subviews: Subviews,cache: inout ()) -> CGSize {
        arrange(width:proposal.width ?? 800,subviews:subviews).size
    }
    func placeSubviews(in bounds: CGRect,proposal: ProposedViewSize,subviews: Subviews,cache: inout ()) {
        let layout=arrange(width:bounds.width,subviews:subviews)
        for (index,point) in layout.points.enumerated() {
            subviews[index].place(at:CGPoint(x:bounds.minX+point.x,y:bounds.minY+point.y),anchor:.topLeading,
                                 proposal:ProposedViewSize(width:min(bounds.width,subviews[index].sizeThatFits(.unspecified).width),height:nil))
        }
    }
    private func arrange(width: CGFloat,subviews: Subviews) -> (size:CGSize,points:[CGPoint]) {
        let width=max(1,width)
        let sizes=subviews.map {view in
            let ideal=view.sizeThatFits(.unspecified)
            return view.sizeThatFits(ProposedViewSize(width:min(width,ideal.width),height:nil))
        }
        return DesktopFlowMetrics.arrange(sizes:sizes,width:width,spacing:spacing)
    }
}
