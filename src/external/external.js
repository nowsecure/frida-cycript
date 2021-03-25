global.choose = function(className) {
  const classToFind = ObjC.classes[className];
  return ObjC.chooseSync(classToFind).map(instance => instance.toString());
};
