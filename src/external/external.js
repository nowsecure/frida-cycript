global.choose = function(className) {
  const classToFind = ObjC.classes[className];
  return ObjC.chooseSync(ObjC.classes[className]).map(instance => instance.toString());
};
