module Node = {
  type context = {
    mutable measureFuncPtr: nativeint,
    mutable contextPtr: nativeint,
  };
  let nullContext = {
    measureFuncPtr: Nativeint.zero,
    contextPtr: Nativeint.zero,
  };
};

module Encoding = FixedEncoding;

module LayoutTypes = LayoutTypes.Create(Node, Encoding);

module Layout = Layout.Create(Node, Encoding);

open Layout.LayoutSupport;

open LayoutTypes;

open Encoding;

/* external caml_c_thread_register : unit => int = "caml_c_thread_register"; */
external cssMeasureFFI:
  (nativeint, int, measureMode, int, measureMode) => dimensions =
  "cssMeasureFFI_bytecode" "cssMeasureFFI";

external caml_thread_initialize: unit => unit = "caml_thread_initialize";

/* Force allocating a new node */
let cssNodeNew = ptr => {
  ...theNullNode,
  style: {
    direction: Inherit,
    flexDirection: Column,
    justifyContent: JustifyFlexStart,
    alignContent: AlignFlexStart,
    alignItems: AlignStretch,
    alignSelf: AlignAuto,
    positionType: Relative,
    flexWrap: CssNoWrap,
    overflow: Visible,
    /* TODO: What is this flex property and why am I paying for it! */
    flex: cssUndefined,
    /* TODO: Fix / check this. */
    flexGrow: cssUndefined,
    flexShrink: cssUndefined,
    /* TODO: Fix / check this */
    flexBasis: cssUndefined,
    marginLeft: cssUndefined,
    marginTop: cssUndefined,
    marginRight: cssUndefined,
    marginBottom: cssUndefined,
    paddingLeft: cssUndefined,
    paddingTop: cssUndefined,
    paddingRight: cssUndefined,
    paddingBottom: cssUndefined,
    borderLeft: cssUndefined,
    borderTop: cssUndefined,
    borderRight: cssUndefined,
    borderBottom: cssUndefined,
    width: cssUndefined,
    height: cssUndefined,
    /* TODO: Fix / check this. (https://github.com/facebook/css-layout) */
    minWidth: cssUndefined,
    minHeight: cssUndefined,
    maxWidth: cssUndefined,
    maxHeight: cssUndefined,
    left: cssUndefined,
    top: cssUndefined,
    right: cssUndefined,
    bottom: cssUndefined,
    start: cssUndefined,
    endd: cssUndefined,
    marginStart: cssUndefined,
    marginEnd: cssUndefined,
    paddingStart: cssUndefined,
    paddingEnd: cssUndefined,
    borderStart: cssUndefined,
    borderEnd: cssUndefined,
    /***
     * All of these need to be reevaluated (to see if we really want them at
     * zero or cssUndefined).
     */
    horizontal: cssUndefined,
    vertical: cssUndefined,
    position: cssUndefined,
    padding: cssUndefined,
    paddingHorizontal: cssUndefined,
    paddingVertical: cssUndefined,
    margin: cssUndefined,
    marginVertical: cssUndefined,
    marginHorizontal: cssUndefined,
    borderHorizontal: cssUndefined,
    borderVertical: cssUndefined,
    border: cssUndefined,
  },
  selfRef: ptr,
  children: [||],
  layout: createLayout(),
  /* Since context is mutated, every node must have its own new copy */
  context: {
    measureFuncPtr: Nativeint.zero,
    contextPtr: Nativeint.zero,
  },
};

let cssNodeSetSelfRef = (node, ptr) => node.selfRef = ptr;

let cssNodeGetSelfRef = node => node.selfRef;

/***
 * TODO: Handle the case where `child` is already a `child` of `node` (`Yoga`
 * doesn't appear to handle this case though).
 */
let cssNodeInsertChild = (node, child, index) => {
  assert(child.parent === theNullNode);
  assert(node.measure === None);
  let capacity = Array.length(node.children);
  if (capacity == node.childrenCount) {
    /* TODO:Simply use Array.fill (no need to allocate a separate `fill` array
     * */
    let fill = Array.make(capacity + 1, theNullNode);
    Array.blit(node.children, 0, fill, 0, capacity);
    node.children = fill;
  };
  for (i in node.childrenCount downto index + 1) {
    node.children[i] = node.children[i - 1];
  };
  node.childrenCount = node.childrenCount + 1;
  node.children[index] = child;
  child.parent = node;
  Layout.LayoutSupport.markDirtyInternal(node);
};

let nodeListRemove = (nodeWithList, index) => {
  let list = nodeWithList.children;
  let removed = list[index];
  list[index] = theNullNode;
  for (i in index to nodeWithList.childrenCount - 2) {
    list[i] = list[i + 1];
    list[i + 1] = theNullNode;
  };
  nodeWithList.childrenCount = nodeWithList.childrenCount - 1;
  removed;
};

let rec nodeListDeleteImpl = (nodeWithList, node, from) =>
  if (from < nodeWithList.childrenCount) {
    if (nodeWithList.children[from] === node) {
      nodeListRemove(nodeWithList, from);
    } else {
      nodeListDeleteImpl(nodeWithList, node, from + 1);
    };
  } else {
    theNullNode;
  };

let nodeListDelete = (nodeWithList, node) =>
  nodeListDeleteImpl(nodeWithList, node, 0);

let cssNodeRemoveChild = (nodeWithList, child) =>
  if (nodeListDelete(nodeWithList, child) !== theNullNode) {
    child.parent = theNullNode;
    Layout.LayoutSupport.markDirtyInternal(nodeWithList);
  };

let cssNodeIsDirty = node => node.isDirty;

let cssNodeChildCount = node => node.childrenCount;

let cssNodeGetChild = (node, i) => {
  let capacity = node.childrenCount;
  if (i >= capacity) {
    Nativeint.zero;
  } else {
    node.children[i].selfRef;
  };
};

let cssNodeCalculateLayout = (node, aw, ah, pd) =>
  Layout.layoutNode(node, aw, ah, pd);

/* New Layout */
Callback.register("YGNodeSetHasNewLayout", (node, hasNewLayout) =>
  node.hasNewLayout = hasNewLayout
);

Callback.register("YGNodeGetHasNewLayout", node => node.hasNewLayout);

/* Style */
Callback.register("YGNodeStyleSetWidth", (node, width) =>
  if (node.style.width != width) {
    markDirtyInternal(node);
    node.style.width = width;
  }
);

Callback.register("YGNodeStyleGetWidth", node => node.style.width);

Callback.register("YGNodeStyleSetHeight", (node, height) =>
  if (node.style.height != height) {
    markDirtyInternal(node);
    node.style.height = height;
  }
);

Callback.register("YGNodeStyleGetHeight", node => node.style.height);

Callback.register("YGNodeStyleSetFlexGrow", (node, flexGrow) =>
  if (node.style.flexGrow != flexGrow) {
    markDirtyInternal(node);
    node.style.flexGrow = flexGrow;
  }
);

Callback.register("YGNodeStyleGetFlexGrow", cssGetFlexGrow);

Callback.register("YGNodeStyleSetFlexShrink", (node, flexShrink) =>
  if (node.style.flexShrink != flexShrink) {
    markDirtyInternal(node);
    node.style.flexShrink = flexShrink;
  }
);

Callback.register("YGNodeStyleGetFlexShrink", cssGetFlexShrink);

Callback.register("YGNodeStyleSetFlexWrap", (node, flexWrap) =>
  if (node.style.flexWrap != flexWrap) {
    markDirtyInternal(node);
    node.style.flexWrap = flexWrap;
  }
);

Callback.register("YGNodeStyleGetFlexWrap", node => node.style.flexWrap);

Callback.register("YGNodeStyleSetJustifyContent", (node, justifyContent) =>
  if (node.style.justifyContent != justifyContent) {
    markDirtyInternal(node);
    node.style.justifyContent = justifyContent;
  }
);

Callback.register("YGNodeStyleGetJustifyContent", node =>
  node.style.justifyContent
);

Callback.register("YGNodeStyleSetAlignItems", (node, alignItems) =>
  if (node.style.alignItems != alignItems) {
    markDirtyInternal(node);
    node.style.alignItems = alignItems;
  }
);

Callback.register("YGNodeStyleGetAlignItems", node => node.style.alignItems);

Callback.register("YGNodeStyleSetAlignContent", (node, alignContent) =>
  if (node.style.alignContent != alignContent) {
    markDirtyInternal(node);
    node.style.alignContent = alignContent;
  }
);

Callback.register("YGNodeStyleGetAlignContent", node =>
  node.style.alignContent
);

Callback.register("YGNodeStyleSetAlignSelf", (node, alignSelf) =>
  if (node.style.alignSelf != alignSelf) {
    markDirtyInternal(node);
    node.style.alignSelf = alignSelf;
  }
);

Callback.register("YGNodeStyleGetAlignSelf", node => node.style.alignSelf);

Callback.register("YGNodeStyleSetMaxWidth", (node, maxWidth) =>
  if (node.style.maxWidth != maxWidth) {
    markDirtyInternal(node);
    node.style.maxWidth = maxWidth;
  }
);

Callback.register("YGNodeStyleGetMaxWidth", node => node.style.maxWidth);

Callback.register("YGNodeStyleSetMaxHeight", (node, maxHeight) =>
  if (node.style.maxHeight != maxHeight) {
    markDirtyInternal(node);
    node.style.maxHeight = maxHeight;
  }
);

Callback.register("YGNodeStyleGetMaxHeight", node => node.style.maxHeight);

Callback.register("YGNodeStyleSetMinWidth", (node, minWidth) =>
  if (node.style.minWidth != minWidth) {
    markDirtyInternal(node);
    node.style.minWidth = minWidth;
  }
);

Callback.register("YGNodeStyleGetMinWidth", node => node.style.minWidth);

Callback.register("YGNodeStyleSetMinHeight", (node, minHeight) =>
  if (node.style.minHeight != minHeight) {
    markDirtyInternal(node);
    node.style.minHeight = minHeight;
  }
);

Callback.register("YGNodeStyleGetMinHeight", node => node.style.minHeight);

Callback.register("YGNodeStyleGetDirection", node => node.style.direction);

Callback.register("YGNodeStyleSetDirection", (node, direction) =>
  if (node.style.direction != direction) {
    markDirtyInternal(node);
    node.style.direction = direction;
  }
);

Callback.register("YGNodeStyleGetFlexDirection", node =>
  node.style.flexDirection
);

Callback.register("YGNodeStyleSetPositionType", (node, positionType) =>
  if (node.style.positionType != positionType) {
    markDirtyInternal(node);
    node.style.positionType = positionType;
  }
);

Callback.register("YGNodeStyleGetPositionType", node =>
  node.style.positionType
);

Callback.register("YGNodeStyleSetFlexDirection", (node, flexDirection) =>
  if (node.style.flexDirection != flexDirection) {
    markDirtyInternal(node);
    node.style.flexDirection = flexDirection;
  }
);

Callback.register("YGNodeStyleGetOverflow", node => node.style.overflow);

Callback.register("YGNodeStyleSetFlexBasis", (node, flexBasis) =>
  if (node.style.flexBasis != flexBasis) {
    markDirtyInternal(node);
    node.style.flexBasis = flexBasis;
  }
);

Callback.register("YGNodeStyleGetFlexBasis", cssGetFlexBasis);

let setIfUndefined = (oldValue, newValue) =>
  if (isUndefined(oldValue)) {
    newValue;
  } else {
    oldValue;
  };

let setIfZero = (oldValue, newValue) =>
  if (0 == oldValue) {
    newValue;
  } else {
    oldValue;
  };

Callback.register("YGNodeStyleSetFlex", (node, flex) =>
  if (node.style.flex != flex) {
    markDirtyInternal(node);
    node.style.flex = flex;
  }
);

Callback.register("YGNodeStyleSetOverflow", (node, overflow) =>
  if (node.style.overflow != overflow) {
    markDirtyInternal(node);
    node.style.overflow = overflow;
  }
);

Callback.register("YGNodeStyleSetPadding", (node, edge, v) => {
  markDirtyInternal(node);
  switch (edge) {
  | Left => node.style.paddingLeft = v
  | Top => node.style.paddingTop = v
  | Right => node.style.paddingRight = v
  | Bottom => node.style.paddingBottom = v
  | Start => node.style.paddingStart = v
  | End => node.style.paddingEnd = v
  | Horizontal => node.style.paddingHorizontal = v
  | Vertical => node.style.paddingVertical = v
  | All => node.style.padding = v
  };
});

Callback.register("YGNodeStyleSetMargin", (node, edge, v) => {
  markDirtyInternal(node);
  switch (edge) {
  | Left => node.style.marginLeft = v
  | Top => node.style.marginTop = v
  | Right => node.style.marginRight = v
  | Bottom => node.style.marginBottom = v
  | Start => node.style.marginStart = v
  | End => node.style.marginEnd = v
  | Horizontal => node.style.marginHorizontal = v
  | Vertical => node.style.marginVertical = v
  | All => node.style.margin = v
  };
});

Callback.register("YGNodeStyleSetBorder", (node, edge, v) => {
  markDirtyInternal(node);
  switch (edge) {
  | Left => node.style.borderLeft = v
  | Top => node.style.borderTop = v
  | Right => node.style.borderRight = v
  | Bottom => node.style.borderBottom = v
  | Start => node.style.borderStart = v
  | End => node.style.borderEnd = v
  | Horizontal => node.style.borderHorizontal = v
  | Vertical => node.style.borderVertical = v
  | All => node.style.border = v
  };
});

Callback.register("YGNodeStyleSetPosition", (node, edge, v) => {
  markDirtyInternal(node);
  switch (edge) {
  | Left => node.style.left = v
  | Top => node.style.top = v
  | Right => node.style.right = v
  | Bottom => node.style.bottom = v
  | Start => node.style.start = v
  | End => node.style.endd = v
  | Horizontal => node.style.horizontal = v
  | Vertical => node.style.vertical = v
  | All => node.style.position = v
  };
});

Callback.register("YGNodeStyleGetPosition", (node, edge) =>
  computedEdgeValuePosition(node.style, edge, cssUndefined)
);

Callback.register("YGNodeStyleGetMargin", (node, edge) =>
  computedEdgeValueMargin(node.style, edge, 0)
);

Callback.register("YGNodeStyleGetBorder", (node, edge) =>
  computedEdgeValueBorder(node.style, edge, 0)
);

Callback.register("YGNodeStyleGetPadding", (node, edge) =>
  computedEdgeValuePadding(node.style, edge, 0)
);

/* Layout */
Callback.register("YGNodeLayoutGetWidth", node => node.layout.width);

Callback.register("YGNodeLayoutGetHeight", node => node.layout.height);

Callback.register("YGNodeLayoutGetTop", node => node.layout.top);

Callback.register("YGNodeLayoutGetBottom", node => node.layout.bottom);

Callback.register("YGNodeLayoutGetLeft", node => node.layout.left);

Callback.register("YGNodeLayoutGetRight", node => node.layout.right);

Callback.register("YGNodeLayoutGetDirection", node => node.layout.direction);

/* Misc */
Callback.register("minInt", () => min_int);

Callback.register("YGNodeCopyStyle", (destNode, srcNode) => {
  markDirtyInternal(destNode);
  destNode.style = srcNode.style;
});

Callback.register("YGNodeNew", cssNodeNew);

Callback.register("YGNodeGetSelfRef", cssNodeGetSelfRef);

Callback.register("YGNodeInsertChild", cssNodeInsertChild);

Callback.register("YGNodeRemoveChild", cssNodeRemoveChild);

Callback.register("YGNodeChildCount", cssNodeChildCount);

Callback.register("YGNodeGetChild", cssNodeGetChild);

Callback.register("YGNodeIsDirty", cssNodeIsDirty);

Callback.register("YGNodeCalculateLayout", cssNodeCalculateLayout);

Callback.register("YGNodeSetContext", (node, context) =>
  node.context.contextPtr = context
);

Callback.register("YGNodeGetContext", node => node.context.contextPtr);

Callback.register("YGNodeSetMeasureFunc", (node, ptr) => {
  node.context.measureFuncPtr = ptr;
  if (node.context.measureFuncPtr != Nativeint.zero) {
    node.measure =
      Some(
        (node, w, wm, h, hm) => cssMeasureFFI(node.selfRef, w, wm, h, hm),
      );
  } else {
    node.measure = None;
  };
});

Callback.register("YGNodeGetMeasureFunc", node => node.context.measureFuncPtr);

Callback.register("GetMeasurement", (width, height) => {width, height});

Callback.register("YGNodeMarkDirty", markDirtyInternal);

Callback.register("initThread", () => {
  ignore(Thread.self());
  caml_thread_initialize();
});
